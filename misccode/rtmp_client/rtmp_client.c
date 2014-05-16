#include <stdio.h>
#include <assert.h>

#include <myclib/all.h>

#include <signal.h>

#include "rtmp.h"
#include "flv.h"

int rtmp_process_client_message(rtmp_state_t *st, rtmp_message_t *msg) {
    int rc, msgid = msg->type_id;

    rc = 0;
    switch (msgid) {
    case RTMP_MSG_SET_CHUNK_SIZE:
        rc = rtmp_set_chunksize(st, msg);
        break;

    case RTMP_MSG_USER_CONTROL:
        rc = rtmp_process_control_message(st, msg);
        break;

    case RTMP_MSG_ABORT_MESSAGE:
    case RTMP_MSG_ACKNOWLEDGE:
    case RTMP_MSG_ACKNOWLEDGE_SIZE:
    case RTMP_MSG_SET_PEER_BANDWIDTH:
        break;

    case RTMP_MSG_CMD_AMF0:
        rc = rtmp_receive_cmd(st, msg);
        break;

    case RTMP_MSG_CMD_AMF3:
    case RTMP_MSG_DATA_AMF0:
    case RTMP_MSG_DATA_AMF3:
    case RTMP_MSG_AUDIO:
    case RTMP_MSG_VIDEO:
    case RTMP_MSG_AGG:
        break;

    default:
        assert(0);
    };

    rtmp_message_free(st, msg);
    return rc;
}

int  rtmp_client_wait_play(rtmp_state_t *st) {
    int rc;
    rtmp_message_t *msg;
    while (st->state != RTMP_PLAYING) {
        rc = rtmp_receive_chunk(st, &msg);
        if (rc == 1 || (rc == 2 && rtmp_process_client_message(st, msg)))
            return 1;
    }
    return 0;
}

static rtmp_state_t *global_st;
void sigint_media_handler(int signum) {
    rtmp_state_free(global_st);
    exit(0);
}

int rtmp_receive_media(rtmp_state_t *st) {
    int rc, flags, to_stop;
    rtmp_message_t *msg;
    my_packet_t pkt;

#ifndef WIN32
    sigset_t curr_set, old_set;

    sigemptyset(&curr_set);
    sigaddset(&curr_set, SIGINT);
#endif

    signal(SIGINT, &sigint_media_handler);

    to_stop = 0;
    global_st = st;

    while (!to_stop) {
        rc = rtmp_receive_chunk(st, &msg);

        if (rc == 1)
            return 1;

        if (!rc)
            continue;
    
        flags = 0;
        switch (msg->type_id) {
        case RTMP_MSG_AUDIO:
            flags = PACKET_TYPE_AUDIO;
            break;
        case RTMP_MSG_VIDEO:
            flags = PACKET_TYPE_VIDEO;
            break;
        case RTMP_MSG_DATA_AMF0:
            flags = PACKET_TYPE_METADATA;
            break;
        default:
            break;
        };

        if (flags) {
            pkt.pts = pkt.dts = msg->timestamp;
            pkt.stream_index = 0;
            pkt.flags = flags;
            pkt.data = msg->body;
            pkt.size = msg->size;

#ifdef WIN32
            sigprocmask(SIG_SETMASK, &curr_set, &old_set);
#endif

            if (my_flv_write(st->flv_context, &pkt)) {
                my_log_error("error writing flv file");
                to_stop = 1;
            }

#ifdef WIN32
            sigprocmask(SIG_SETMASK, &old_set, &curr_set);
#endif
        }

        rtmp_message_free(st, msg);
    }

    return 0;
}

int main(int argc, char **argv) {
    rtmp_state_t st;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <url>\n", argv[0]);
        return 1;
    }

    logger_init("/dev/stderr");
    /* set_logger_level(LOG_DEBUG); */
    set_logger_level(LOG_INFO);

    st.remote_url = argv[1];
    st.flv_path = "dump.flv";

    if (rtmp_state_init(&st)) {
        my_log_error("rtmp_state_init() failed");
        return 1;
    }

    if (rtmp_handshake(&st)) {
        my_log_error("handshake is unsuccessful");
        return 1;
    }

    my_log_info("handshake is successful");

    if (rtmp_send_connect(&st)) {
        my_log_error("connect failed");
        return 1;
    }

    if (rtmp_client_wait_play(&st))
        return 1;

    my_log_info("ready to play");

    if (rtmp_receive_media(&st)) {
        my_log_info("error getting media");
        return 1;
    }

    rtmp_state_free(&st);
    return 0;
}

