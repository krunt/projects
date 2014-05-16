#include "rtmp.h"
#include "amf0.h"
#include "flv.h"

#include <string.h>
#include <assert.h>

void rtmp_message_init(rtmp_message_t *msg);

int rtmp_parse_uri(rtmp_state_t *st) {
    char *b, *e;
    char *p = st->remote_url;

    if (strncmp(p, "rtmp://", 7) == 0)
        p += 7;

    b = p;
    for (; *p && !(*p == ':' || *p == '/'); ++p);

    st->host = malloc(p - b + 1);
    memcpy(st->host, b, p - b);
    st->host[p - b] = 0;

    if (!*p) {
        my_log_debug("expected `:' or `/' in url, eol found");
        return 1;
    }

    if (*p == ':') {
        st->port = strtol(++p, &e, 10);
        p = e;
    } 

    if (!*p || *p != '/') {
        my_log_debug("expected `/' in url, eol found");
        return 1;
    }

    b = ++p;
    for (; *p && *p != '/'; ++p);

    if (!*p) {
        my_log_debug("expected `/' in playpath");
        return 1;
    }

    *p++ = 0;
    st->app_name = strdup(b);
    st->stream_name = strdup(p);

    return 0;
}

int rtmp_state_init(rtmp_state_t *st) 
{
    st->state = RTMP_INIT;
    st->chunk_size = 128;

    if (rtmp_parse_uri(st))
        return 1;

    if (!(st->proto = my_url_open("tcp", st->host, st->port))) {
        my_log_info("can't connect to `%s':%d", st->host, st->port);
        return 1;
    }

    for (int i = 0; i < RMTP_CHUNK_CHANNEL_COUNT; ++i) {
        INIT_LIST_HEAD(&st->in_channels[i].msg_pending);
        rtmp_message_init(&st->in_channels[i].last_message);
    }

    st->read_buf = NULL;
    st->read_buf_size = 0;
    st->stream_ready = 0;
    st->tx_seq = 0;

    if (!(st->flv_context = my_flv_open(st->flv_path, "w"))
            || my_flv_write_header(st->flv_context))
        return 1;

    return 0;
}

void rtmp_state_free(rtmp_state_t *st) {
    if (st->host)
        free(st->host);
    free(st->app_name);
    free(st->stream_name);
    if (st->read_buf)
        free(st->read_buf);
    if (st->flv_context)
        my_flv_close(st->flv_context); 
}

rtmp_message_t *rmtp_message_new(rtmp_state_t *st) {
    rtmp_message_t *msg = malloc(sizeof(rtmp_message_t));
    rtmp_message_init(msg);
    return msg;
}

void rtmp_message_init(rtmp_message_t *msg) {
    INIT_LIST_HEAD(&msg->node);
    msg->timestamp = 0;
    msg->timestamp_delta = 0;
    msg->is_timestamp_extended = 0;
    msg->length = 0; 
    msg->type_id = 0;
    msg->streamid = 0;
    msg->body = NULL;
    msg->size = 0;
}

void rtmp_message_free(rtmp_state_t *st, rtmp_message_t *msg) {
    list_del(&msg->node);
    free(msg);
}

static rtmp_message_t *rtmp_message_find(rtmp_state_t *st, 
        int chunk_id, int msg_id) 
{
    rtmp_message_t *msg;
    rtmp_chunk_channel_t *ch = &st->in_channels[chunk_id];

    list_for_each_entry(msg, &ch->msg_pending, node) {
        if (msg->streamid == msg_id) {
            return msg;
        }
    }
    return NULL;
}

static u8 *rtmp_read(rtmp_state_t *st, int n) {
    if (!st->read_buf || st->read_buf_size < n) {
        st->read_buf = realloc(st->read_buf, n);
        st->read_buf_size = n;
    }
    if (my_url_read(st->proto, st->read_buf, n))
        return NULL;
    return st->read_buf;
}

#define CHECK_CALL(x) \
    if (!(x)) { return 1; }

int rtmp_receive_chunk(rtmp_state_t *st, rtmp_message_t **msgout) 
{
    int rc, chunk_type, chunk_id, skip, to_read;
    u32 msg_timestamp = 0, msg_timestamp_delta = 0,
        msg_length = 0, msg_typeid = 0, msg_streamid = 0,
        msg_ts_extended = 0;
    rtmp_message_t *msg, *msg1;
    rtmp_chunk_channel_t *ch;
    u8 *p;

    CHECK_CALL(p = rtmp_read(st, 1));

    chunk_type= (*p & 0xC0) >> 6;
    chunk_id = *p & 0x3F;

    if (!chunk_id) {
        CHECK_CALL(p = rtmp_read(st, 1));
        chunk_id = *p + 64;
    } 
    else if (chunk_id == 1) {
        CHECK_CALL(p = rtmp_read(st, 2));
        chunk_id = p[1] * 256 + p[0] + 64;
    }

    my_log_debug("got chunk_id=%d, chunk_type=%d", chunk_id, chunk_type);

    if (chunk_id >= RMTP_CHUNK_CHANNEL_COUNT) {
        my_log_error("got invalid chunk_id %d > %d, ignoring...",
                chunk_id, RMTP_CHUNK_CHANNEL_COUNT);
        return 1;
    }

    ch = &st->in_channels[chunk_id];
    switch (chunk_type) {
    case 0:
        CHECK_CALL(p = rtmp_read(st, 11));

        rtmp_log_hex("received chunk header", p, 11);

        unpack3_be(&msg_timestamp, p); p += 3;
        unpack3_be(&msg_length, p); p += 3;
        unpack1_be(&msg_typeid, p); p++;
        unpack4_le(&msg_streamid, p); p += 4;

        if (msg_timestamp == 0xffffff) {
            CHECK_CALL(p = rtmp_read(st, 4));
            unpack4_be(&msg_timestamp, p);
            msg_ts_extended = 1;
        }

        msg_timestamp_delta = 0;
        break;

    case 1:
        CHECK_CALL(p = rtmp_read(st, 7));

        rtmp_log_hex("received chunk header", p, 7);

        unpack3_be(&msg_timestamp_delta, p); p += 3;
        unpack3_be(&msg_length, p); p += 3;
        unpack1_be(&msg_typeid, p); p++;

        if (msg_timestamp_delta == 0xffffff) {
            CHECK_CALL(p = rtmp_read(st, 4));
            unpack4_be(&msg_timestamp_delta, p);
            msg_ts_extended = 1;
        }

        msg1 = &ch->last_message;
        msg_timestamp = msg_timestamp_delta + msg1->timestamp;
        msg_streamid = msg1->streamid;
        break;

    case 2:
        CHECK_CALL(p = rtmp_read(st, 3));

        rtmp_log_hex("received chunk header", p, 3);

        unpack3_be(&msg_timestamp_delta, p); p += 3;

        if (msg_timestamp_delta == 0xffffff) {
            CHECK_CALL(p = rtmp_read(st, 4));
            unpack4_be(&msg_timestamp_delta, p);
            msg_ts_extended = 1;
        }

        msg1 = &ch->last_message;
        msg_timestamp = msg_timestamp_delta + msg1->timestamp;
        msg_length = msg1->length;
        msg_typeid = msg1->type_id;
        msg_streamid = msg1->streamid;
        break;

    case 3:
        msg1 = &ch->last_message;

        if (msg1->is_timestamp_extended) {
            CHECK_CALL(p = rtmp_read(st, 4));
            unpack4_be(&msg_timestamp_delta, p);
            msg_timestamp = msg1->timestamp + msg_timestamp_delta;
            msg_ts_extended = 1;
        } else {
            msg_timestamp = msg1->timestamp + msg1->timestamp_delta;
        }

        msg_timestamp_delta = msg1->timestamp_delta;
        msg_length = msg1->length;
        msg_typeid = msg1->type_id;
        msg_streamid = msg1->streamid;
        break;

    default:
        assert(0);
    };

    msg = rtmp_message_find(st, chunk_id, msg_streamid);

    if (!msg) {
        msg = rmtp_message_new(st);
        msg->size = 0;

        if (!(msg->body = malloc(msg_length)))
            return 1;

        list_add(&msg->node, &ch->msg_pending);
    }

    msg->timestamp = msg_timestamp;
    msg->timestamp_delta = msg_timestamp_delta;
    msg->is_timestamp_extended = msg_ts_extended;
    msg->length = msg_length;
    msg->type_id = msg_typeid;
    msg->streamid = msg_streamid;
    msg->chunk_id = chunk_id;

    ch->last_message = *msg;

    to_read = min_t(int, st->chunk_size, msg->length - msg->size);
    CHECK_CALL(p = rtmp_read(st, to_read));

    memcpy(msg->body + msg->size, p, to_read);

    rtmp_log_hex("received chunk body", p, to_read);

    msg->size += to_read;

    /* message is read */
    if (msg->size == msg->length) {
        my_log_debug("state=%d", st->state);
        my_log_debug("msg_length=%d,msg_typeid=%d,msg_streamid=%d,msg_chunk_id=%d", 
            msg->length, msg->type_id, msg->streamid, msg->chunk_id);
        rtmp_log_hex("received message", msg->body, msg->length);
        *msgout = msg;
        return 2;
    }

    return 0;
}

static u8 *encode_chunkstream_id(u8 *p, int chunk_type, int chunk_id) {
    if (chunk_id < 64) {
        *p++ = (chunk_type << 6) | chunk_id; 
    } else if (chunk_id < 320) {
        *p++ = (chunk_type << 6);
        *p++ = chunk_id - 64;
    } else {
        *p++ = (chunk_type << 6) | 0x3f;
        chunk_id -= 64;
        pack2_be(p, &chunk_id); p += 2;
    }
    return p;
}

static int rtmp_encode_chunk(rtmp_state_t *st, rtmp_message_t *msg, 
        int *bytes_written, int *extended_timestamp) 
{
    u8 *p, header[128];
    int to_write;

    p = header;
    if (!*bytes_written) {
        /* chunk type 0 */
        p = encode_chunkstream_id(p, 0, msg->chunk_id);

        if (msg->timestamp > 0xffffff) {
            msg->timestamp = 0xffffff;
            *extended_timestamp = msg->timestamp;
        }

        pack3_be(p, &msg->timestamp); p += 3;
        pack3_be(p, &msg->length); p += 3;
        pack1_be(p, &msg->type_id); p++;
        pack4_le(p, &msg->streamid); p += 4;

    } else {
        /* chunk type 3 */
        p = encode_chunkstream_id(p, 3, msg->chunk_id);
    }

    if (*extended_timestamp) {
        pack4_be(p, extended_timestamp); p += 4;
    }

    to_write = min_t(int, st->chunk_size, msg->length - *bytes_written);

    rtmp_log_hex("chunk-header", header, p - header);
    rtmp_log_hex("chunk-body", msg->body + *bytes_written, to_write);

    if (my_url_write(st->proto, header, p - header)
        || my_url_write(st->proto, msg->body + *bytes_written,
            to_write))
    {
        return 1;
    }

    *bytes_written += to_write;
    return 0;
}

int rtmp_encode_message(rtmp_state_t *st, rtmp_message_t *msg)
{
    int written, extended_timestamp;

    /*
    rtmp_chunk_channel_t *ch = &st->out_channels[chunk_stream_id];
    assert(!ch->last_message);
    */

    written = 0;
    extended_timestamp = 0;
    while (written < msg->length) {
        if (rtmp_encode_chunk(st, msg,
                    &written, &extended_timestamp))
            return 1;
    }
    return 0;
}

int rtmp_handshake(rtmp_state_t *st) {
    int i;
    u8 *p, c0[1], c1[1536], c2[1536];
    u8 s0[1], s1[1536], s2[1536];
    my_url_proto_t *prot = st->proto;

    c0[0] = RTMP_VERSION;
    CHECK_CALL(my_url_write(prot, c0, sizeof(c0)) == 0);

    p = c1;
    memset(p, 0, 8); p += 8;
    for (i = 0; i < 1528; ++i) {
        p[i] = rand() & 0xFF;
    }

    CHECK_CALL(my_url_write(prot, c1, sizeof(c1)) == 0);
    CHECK_CALL(my_url_read(prot, s0, sizeof(s0)) == 0);
    CHECK_CALL(my_url_read(prot, s1, sizeof(s1)) == 0);

    if (s0[0] != c0[0]) {
        my_log_debug("version mismatch %d != %d\n", c0[0], s0[0]);
        return 1;
    }
    
    memcpy(c2, s1, sizeof(c2));
    CHECK_CALL(my_url_write(prot, c2, sizeof(c2)) == 0);
    CHECK_CALL(my_url_read(prot, s2, sizeof(s2)) == 0);

    if (memcmp(c2, s2, sizeof(c2)) != 0) {
        my_log_debug("client signature doesn't not match");
        return 1;
    }

    st->state = RTMP_HANDSHAKE_DONE;
    return 0;
}

int rtmp_send_connect(rtmp_state_t *st) {
    u8 *p, *end, buf[16384];
    rtmp_message_t msg;
    amf0_value_t *obj;

    rtmp_message_init(&msg);
    msg.type_id = RTMP_MSG_CMD_AMF0;
    msg.streamid = 3;
    msg.chunk_id =  RTMP_USER_CHUNK_STREAM_ID;

    msg.body = buf;
    msg.length = 0;

    p = buf;
    end = p + sizeof(buf);

    my_log_debug("sending connect message");

    CHECK_CALL(amf0_encode_string(obj = amf0_create_string("connect"), 
                &p, end) == 0);
    amf0_free(obj);

    CHECK_CALL(amf0_encode_number(obj = amf0_create_number(1.0), 
                &p, end) == 0);
    amf0_free(obj);

    obj = amf0_create_object();
    amf0_object_insert(obj, amf0_create_string("app"), 
            amf0_create_string(st->app_name));
    amf0_object_insert(obj, amf0_create_string("flashver"), 
            amf0_create_string("FMLE/3.0"));
    amf0_object_insert(obj, amf0_create_string("tcUrl"), 
            amf0_create_string(st->remote_url));
    amf0_object_insert(obj, amf0_create_string("fpad"), 
            amf0_create_boolean(0));
    amf0_object_insert(obj, amf0_create_string("capabilities"), 
            amf0_create_number(15.0));
    amf0_object_insert(obj, amf0_create_string("audioCodecs"), 
            amf0_create_number(1639.0));
    amf0_object_insert(obj, amf0_create_string("videoCodecs"), 
            amf0_create_number(252.0));
    amf0_object_insert(obj, amf0_create_string("videoFunction"), 
            amf0_create_number(1.0));
    amf0_object_insert(obj, amf0_create_string("objectEncoding"), 
            amf0_create_number(0.0));

    CHECK_CALL(amf0_encode_object(obj, &p, end) == 0);
    amf0_free(obj);

    CHECK_CALL(amf0_encode_object(amf0_create_object(), &p, end) == 0);
    amf0_free(obj);

    msg.length = p - buf;

    rtmp_log_hex("connect_packet", msg.body, msg.length);
    CHECK_CALL(rtmp_encode_message(st, &msg) == 0);

    my_log_debug("sent connect message successfully");

    return 0;
}


int rtmp_set_chunksize(rtmp_state_t *st, rtmp_message_t *msg) 
{
    if (msg->length < 4)
        return 1;
    unpack4_be(&st->chunk_size, msg->body);
    my_log_debug("set chunk_size to %d", st->chunk_size);
    return 0;
}

int rtmp_receive_cmd(rtmp_state_t *st, rtmp_message_t *msg) 
{
    u8 *p, *end;

    p = msg->body;
    end = p + msg->length;

    if (end - p < 7)
        return 1;

    if (!memcmp(p, "_error", 6)) {
        my_log_debug("received _error for last cmd");
        return 1;
    }

    switch (st->state) {
    case RTMP_HANDSHAKE_DONE:
    case RTMP_HANDSHAKE_DONE2:
        if (rtmp_send_create_stream(st))
            return 1;
        st->state = RTMP_CONNECTED;
        /*
        if (st->stream_ready) {
            if (rtmp_send_create_stream(st))
                return 1;
            st->state = RTMP_CONNECTED;
        } else if (st->state == RTMP_HANDSHAKE_DONE) {
            st->state = RTMP_HANDSHAKE_DONE2;
        }
        */
        break;

    case RTMP_CONNECTED:
        if (rtmp_send_play(st))
            return 1;
        st->state = RTMP_READY_TO_PLAY;
        break;

    case RTMP_READY_TO_PLAY:
        st->state = RTMP_PLAYING;
        break;

    case RTMP_PLAYING:
        break;

    default: assert(0);
    };

    return 0;
}

int rtmp_process_control_message(rtmp_state_t *st, rtmp_message_t *msg)
{
    u16 event_type;

    if (msg->streamid != 0 || msg->chunk_id != 2) {
        my_log_debug("unexpected control message with %d/%d",
                msg->streamid, msg->chunk_id);
        return 1;
    }

    unpack2_be(&event_type, msg->body);

    switch (event_type) {
    case RTMP_EVENT_STREAM_BEGIN:
        st->stream_ready = 1;
        /*
        if (st->state == RTMP_HANDSHAKE_DONE2) {
            if (rtmp_send_create_stream(st))
                return 1;
            st->state = RTMP_CONNECTED;
        }
        */
        break;

    case RTMP_EVENT_STREAM_EOF:
    case RTMP_EVENT_STREAM_DRY:
        st->stream_ready = 0;
        break;

    case RTMP_EVENT_STREAM_PING_REQUEST:
        if (rtmp_send_pong(st, msg))
            return 1;
        break;

    default:
        break;
    };

    return 0;
}

int rtmp_send_play(rtmp_state_t *st) {
    u8 *p, *end, buf[2048];
    rtmp_message_t msg;
    amf0_value_t *obj;

    rtmp_message_init(&msg);
    msg.type_id = RTMP_MSG_CMD_AMF0;
    msg.streamid = 0;
    msg.chunk_id =  RTMP_USER_CHUNK_STREAM_ID;

    msg.body = buf;
    msg.length = 0;

    p = buf;
    end = p + sizeof(buf);

    my_log_debug("sending play-command message");

    CHECK_CALL(amf0_encode_string(obj = amf0_create_string("play"), 
                &p, end) == 0);
    amf0_free(obj);

    CHECK_CALL(amf0_encode_number(obj = amf0_create_number(0), &p, end) == 0);
    amf0_free(obj);

    CHECK_CALL(amf0_encode_null(&p, end) == 0);

    CHECK_CALL(amf0_encode_string(obj = amf0_create_string((u8*)st->stream_name), 
                &p, end) == 0);
    amf0_free(obj);

    msg.length = p - buf;

    rtmp_log_hex("send-play packet", msg.body, msg.length);
    CHECK_CALL(rtmp_encode_message(st, &msg) == 0);
    my_log_debug("sending play-command done");

    return 0;
}

int rtmp_send_create_stream(rtmp_state_t *st) {
    u8 *p, *end, buf[1024];
    rtmp_message_t msg;
    amf0_value_t *obj;

    rtmp_message_init(&msg);
    msg.type_id = RTMP_MSG_CMD_AMF0;
    msg.streamid = 0;
    msg.chunk_id =  RTMP_USER_CHUNK_STREAM_ID;

    msg.body = buf;
    msg.length = 0;

    p = buf;
    end = p + sizeof(buf);

    my_log_debug("sending create-stream message");

    CHECK_CALL(amf0_encode_string(obj = amf0_create_string("createStream"), 
                &p, end) == 0);
    amf0_free(obj);

    CHECK_CALL(amf0_encode_number(obj = amf0_create_number(st->tx_seq++),
                &p, end) == 0);
    amf0_free(obj);

    CHECK_CALL(amf0_encode_null(&p, end) == 0);

    msg.length = p - buf;

    rtmp_log_hex("create-stream packet", msg.body, msg.length);
    CHECK_CALL(rtmp_encode_message(st, &msg) == 0);
    my_log_debug("sending create-stream done");

    return 0;
}

int rtmp_send_pong(rtmp_state_t *st, rtmp_message_t *m) {
    u8 b[6], type = 7;
    rtmp_message_t msg;

    pack2_be(b, &type);
    memcpy(&b[2], m->body + 2, 4);

    /* TODO:  timestamp */

    rtmp_message_init(&msg);
    msg.type_id = RTMP_MSG_USER_CONTROL;
    msg.streamid = 0;
    msg.chunk_id = 2;
    msg.body = b;
    msg.length = 6;

    rtmp_log_hex("pong packet", msg.body, msg.length);
    CHECK_CALL(rtmp_encode_message(st, &msg) == 0);
    my_log_debug("sending pong-packet done");

    return 0;
}
