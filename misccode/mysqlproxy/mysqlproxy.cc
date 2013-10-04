#include "client_priv.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <violite.h>

#include <event.h>

namespace {
struct fragment_t {
    fragment_t():
        fragment_size(0), fragment_seq_id(0),
        size_determined(false),
        pending_size(0)
    { init_dynamic_string(&fragment_data, "", 128, 16); }

    ~fragment_t() {
        dynstr_free(&fragment_data);
    }

    char *append(char *data, size_t size) {
        if (!size_determined) {
            if (size + pending_size >= 4) {
                size_determined = true; 
                memcpy(&pending_buf[pending_size], data, 4 - pending_size);

                size_t bytes_to_write_to_pending = 4 - pending_size;
                data += bytes_to_write_to_pending;
                size -= bytes_to_write_to_pending;
                pending_size = 4;

                fragment_size = uint3korr(pending_buf);
                fragment_seq_id = (uchar)pending_buf[3];

                return append(data, size);
            } else {
                memcpy(&pending_buf[pending_size], data, size);
                pending_size += size;
                return data + size;
            }
        }

        if (fragment_data.length + size < fragment_size) {
            dynstr_append_mem(&fragment_data, size);
            return data + size;
        } else {
            size_t bytes_to_fill = fragment_size - fragment_data.length;
            dynstr_append_mem(&fragment_data, data, bytes_to_fill);
            return data + bytes_to_fill;
        }
    }

    char *get_pointer() const {
        return fragment_data.str;
    }

    size_t get_size() const {
        return fragment_size;
    }

    bool is_complete() const {
        return fragment_size == fragment_data.length;
    }

    bool is_last() const {
        return fragment_size != MAX_PACKET_LENGTH;
    }

private:
    size_t fragment_size;
    uchar fragment_seq_id;
    DYNAMIC_STRING fragment_data;

    bool size_determined;
    size_t pending_size;
    char pending_buf[4];
};

struct compressed_fragment_t {
    compressed_fragment_t():
        compressed_fragment_size(0),
        fragment_seq_id(0),
        uncompressed_fragment_size(0),
        completed(false),
        size_determined(false),
        pending_size(0)
    { init_dynamic_string(&fragment_data, "", 128, 16); }

    ~compressed_fragment_t() {
        dynstr_free(&fragment_data);
    }

    char *append(char *data, size_t size) {
        if (!size_determined) {
            if (size + pending_size >= 7) {
                size_determined = true; 
                memcpy(&pending_buf[pending_size], data, 7 - pending_size);

                size_t bytes_to_write_to_pending = 7 - pending_size;
                data += bytes_to_write_to_pending;
                size -= bytes_to_write_to_pending;
                pending_size = 7;

                compressed_fragment_size = uint3korr(pending_buf);
                fragment_seq_id = (uchar)pending_buf[3];
                uncompressed_fragment_size = uint3korr(pending_buf + 3 + 1);

                return append(data, size);
            } else {
                memcpy(&pending_buf[pending_size], data, size);
                pending_size += size;
                return data + size;
            }
        }

        if (fragment_data.length + size < compressed_fragment_size) {
            dynstr_append_mem(&fragment_data, data, size);
            return data + size;
        } else {
            size_t bytes_to_fill = compressed_fragment_size - fragment_data.length;
            dynstr_append_mem(&fragment_data, data, bytes_to_fill);
            finish_fragment();
            return data + bytes_to_fill;
        }
    }

    char *get_pointer() const {
        return fragment_data.str;
    }

    size_t get_compressed_size() const {
        return compressed_fragment_size;
    }

    size_t get_uncompressed_size() const {
        return uncompressed_fragment_size;
    }

    bool is_complete() const {
        return completed;
    }

    bool is_last() const {
        return get_uncompressed_size() != MAX_PACKET_LENGTH - 4;
    }

private:
    void finish_fragment() {
        completed = true;
        dynstr_realloc(&fragment_data, 
            uncompressed_fragment_size - compressed_fragment_size);
        my_uncompress(fragment_data.str, compressed_fragment_size,
                &uncompressed_fragment_size); 
    }

    size_t compressed_fragment_size;
    uchar fragment_seq_id;
    size_t uncompressed_fragment_size;
    bool completed;
    DYNAMIC_STRING fragment_data;

    bool size_determined;
    size_t pending_size;
    char pending_buf[7];
};

struct packet_t {
    packet_t()
    { init_dynamic_string(&fragment_data, "", 128, 16); }

    ~packet_t() {
        dynstr_free(&packet_data);
    }

    void append(fragment_t *fragment) {
        dynstr_append_mem(&packet_data, fragment->get_pointer(),
                fragment->get_size());
    }

    char *get_pointer() const {
        return packet_data.str;
    }

    size_t get_size() const {
        return packet_data.length;
    }

private:
    DYNAMIC_STRING packet_data;
};
} /* namespace */

/* constants */
const char *k_mysql_server = "127.0.0.1";
const int k_proxy_listen_port = 19999;
const int k_mysql_server_port = 3306;

enum kFlags {
    HANDSHAKE_BEGIN,
    HANDSHAKE_AFTER_SSL,
    HANDSHAKE_COMMAND_PHASE,
};

enum ClientFlags {
    CLIENT_NONE,
    AWAIT_INITIAL_HANDSHAKE_FROM_SERVER,  
    SSL_CONNECTION_REQUEST,
    HANDSHAKE_RESPONSE_PACKET,
    STATUS_PACKET_FROM_SERVER,
    CLIENT_COMMAND_PHASE,
};

enum ServerFlags {
    SERVER_NONE,
    INITIAL_HANDSHAKE_FROM_SERVER,  
    AWAIT_SSL_CONNECTION_REQUEST,
    AWAIT_HANDSHAKE_RESPONSE_PACKET,
    AWAIT_STATUS_PACKET_FROM_SERVER,
    SERVER_COMMAND_PHASE,
};

#define MAX_PACKET_LENGTH (256L*256L*256L-1)
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#define MAX(A,B) ((A) > (B) ? (A) : (B))

class packet_output_stream {
public:
    virtual void feed_packet(uchar *data, size_t len) = 0;
    virtual void reset_packets() = 0;
    virtual void get_packets(char **packet_begin, size_t *write_size) = 0;
    virtual int  packet_count() const = 0;

    void feed_packet(packet_t *packet) { 
        feed_packet(packet->get_pointer(), packet->get_size());    
    }
};

class mysql_packet_output_stream: public packet_output_stream {
public:
    mysql_packet_output_stream() 
        : output_packet_count(0), 
          output_packet_fragments_count(0),
          output_seq(0) 
    { init_dynamic_string(&output, "", 128, 16); }

    ~mysql_packet_output_stream() { 
        dynstr_free(&output);
    }

    void feed_packet(uchar *data, size_t len) {
        uchar packet_header[3+1];
        while (len >= MAX_PACKET_LENGTH) {
            int3store(packet_header, MAX_PACKET_LENGTH);
            packet_header[3] = (uchar)output_seq;
            dynstr_append_mem(&output, packet_header, sizeof(packet_header));
            dynstr_append_mem(&output, data, MAX_PACKET_LENGTH);

            data += MAX_PACKET_LENGTH;
            len -= MAX_PACKET_LENGTH;

            output_packet_fragments_count++;
        }

        int3store(packet_header, len);
        packet_header[3] = (uchar)output_seq++;
        dynstr_append_mem(&output, packet_header, sizeof(packet_header));
        dynstr_append_mem(&output, data, len);

        output_packet_fragments_count++;
        output_packet_count++;
    }

    void reset_packets() {
        output.length = 0;
        output_packet_count = 0;
        output_packet_fragments_count = 0;
    }

    void get_packets(char **packet_begin, size_t *write_size) {
        *packet_begin = output.str;
        *write_size = output.length;
    }

    int  packet_count() const {
        return output_packet_count;
    }

private:
    DYNAMIC_STRING output;
    int output_packet_count;
    int output_packet_fragments_count;
    int output_seq;
};

class mysql_packet_output_stream_compressed: public packet_output_stream {
public:
    mysql_packet_output_stream_compressed() 
        : output_packet_count(0), 
          output_packet_fragments_count(0),
          output_seq(0),
          compressed_output_seq(0)
    { init_dynamic_string(&output, "", 128, 16); }

    ~mysql_packet_output_stream_compressed() { 
        dynstr_free(&output);
    }

    void feed_packet(uchar *data, size_t len) {
        while (len >= MAX_PACKET_LENGTH) {
            size_t uncompressed_size = 3+1+MAX_PACKET_LENGTH;
            uchar *uncompressed_packet_data 
                = (uchar*)malloc(uncompressed_packet_size);
            int3store(uncompressed_packet_data, MAX_PACKET_LENGTH);
            uncompressed_packet_data[3] = (uchar)output_seq;
            memcpy(uncompressed_packet_data, data, MAX_PACKET_LENGTH);
            append_packet_to_output(uncompressed_packet_data, uncompressed_size);
            free(uncompressed_packet_data);

            data += MAX_PACKET_LENGTH;
            len -= MAX_PACKET_LENGTH;
        }

        size_t uncompressed_size = 3+1+len;
        uchar *uncompressed_packet_data 
            = (uchar*)malloc(uncompressed_packet_size);
        int3store(uncompressed_packet_data, len);
        uncompressed_packet_data[3] = (uchar)output_seq++;
        memcpy(uncompressed_packet_data, data, len);
        append_packet_to_output(uncompressed_packet_data, uncompressed_size);
        free(uncompressed_packet_data);
    }

    void reset_packets() {
        output.length = 0;
        output_packet_count = 0;
        output_packet_fragments_count = 0;
    }

    void get_packets(char **packet_begin, size_t *write_size) {
        *packet_begin = output.str;
        *write_size = output.length;
    }

    int  packet_count() const {
        return output_packet_count;
    }

private:
    void append_packet_to_output(uchar *uncompressed_packet_data, 
            size_t uncompressed_size) 
    {
        if (uncompressed_size >= MAX_PACKET_LENGTH - 4) {
            size_t uncompressed_packet_size = MAX_PACKET_LENGTH - 4;
            size_t uncompressed_packet_original = 0;

            my_compress(uncompressed_packet_data, &uncompressed_packet_size,
                &uncompressed_packet_original);
             
            uchar compressed_packet_header[3+1+3];
            int3store(compressed_packet_header, uncompressed_packet_size);
            compressed_packet_header[3] = (uchar)compressed_output_seq;
            int3store(compressed_packet_header+3+1, uncompressed_packet_original);

            dynstr_append_mem(output, compressed_packet_header, 
                sizeof(compressed_packet_header));
            dynstr_append_mem(output,
                uncompressed_packet_data, uncompressed_packet_size);
            output_packet_fragments_count++;

            uncompressed_packet_data += uncompressed_packet_original;
            uncompressed_size -= MAX_PACKET_LENGTH - 4;
        }

        size_t uncompressed_packet_size = uncompressed_size;
        size_t uncompressed_packet_original = 0;

        my_compress(uncompressed_packet_data, &uncompressed_packet_size,
            &uncompressed_packet_original);
         
        uchar compressed_packet_header[3+1+3];
        int3store(compressed_packet_header, uncompressed_packet_size);
        compressed_packet_header[3] = (uchar)compressed_output_seq++;
        int3store(compressed_packet_header+3+1, uncompressed_packet_original);

        dynstr_append_mem(output, compressed_packet_header, 
            sizeof(compressed_packet_header));
        dynstr_append_mem(output,
            uncompressed_packet_data, uncompressed_packet_size);

        output_packet_fragments_count++;
        output_packet_count++;
    }

private:
    DYNAMIC_STRING output;
    int output_packet_count;
    int output_packet_fragments_count;
    int output_seq;
    int compressed_output_seq;
};


class packet_input_stream {
public:
    virtual void feed_data(uchar *data, size_t len) = 0;
    virtual int  packet_count() const = 0;
};

/* TODO: leaking memory when freeing list */
class mysql_packet_input_stream: public packet_input_stream  {
public:
    mysql_packet_input_stream() 
        : output_packets(NULL),
          pending_fragments(NULL)
    {}

    ~mysql_packet_input_stream() {
        list_free(output_packets, 0);
        list_free(pending_fragments, 0);
        output_packets = pending_fragments = NULL;
    }

    void feed_data(uchar *data, size_t len) {
        if (!pending_fragments) {
            list_push(pending_fragments, new fragment_t());
        }

        while (len) {
            uchar *newdata = pending_fragments.back()->append(data, len);
            len -= newdata - data;
            data = newdata;

            fragment_t *fragment 
                = reinterpret_cast<fragment_t *>(pending_fragments->data);
            if (fragment->is_complete()) {
                add_fragment_to_output(fragment);
                list_push(pending_fragments, new fragment_t());
            }
        }
    }

    packet_t *packet_top() const {
        assert(packet_count() > 0);
        packet_t *packet 
            = reinterpret_cast<packet_t *>(output_packets->data);
        return packet;
    }

    void packet_pop() {
        list_pop(output_packets);
    }

    int packet_count() const {
        return list_length(output_packets);
    }

private:
    void add_fragment_to_output(fragment_t *fragment) {
        assert(fragment->is_complete());
        list_push(pending_fragments, fragment);
        if (fragment->is_last()) {
            flush_pending_fragments_to_output();
        }
    }

    void flush_pending_fragments_to_output() {
        packet_t *packet = new packet_t();
        list_reverse(pending_fragments);
        for (LIST *m = pending_fragments; m; m = list_rest(m)) {
            fragment_t *fragment 
                = reinterpret_cast<fragment_t *>(m->data);
            packet->append(fragment);
        }
        list_push(output_packets, packet);
        list_free(pending_fragments, 0);
        pending_fragments = NULL;
    }

private:
    LIST *output_packets;
    LIST *pending_fragments;
};


class mysql_packet_input_stream_compressed: public packet_input_stream {
public:
    mysql_packet_input_stream() 
        : output_packets(NULL),
          pending_fragments(NULL),
          compressed_pending_fragments(NULL)
    {}

    ~mysql_packet_input_stream() {
        list_free(output_packets, 0);
        list_free(pending_fragments, 0);
        list_free(compressed_pending_fragments, 0);
        output_packets = pending_fragments 
            = compressed_pending_fragments = NULL;
    }

    void feed_data(uchar *data, size_t len) {
        if (!compressed_pending_fragments) {
            list_push(compressed_pending_fragments, 
                new compressed_fragment_t());
        }

        while (len) {
            uchar *newdata = compressed_pending_fragments.back()->append(data, len);
            len -= newdata - data;
            data = newdata;

            compressed_fragment_t *fragment 
                = reinterpret_cast<compressed_fragment_t *>(
                        pending_fragments->data);
            if (fragment->is_complete()) {
                add_fragment_to_pending(fragment);
                list_push(compressed_pending_fragments, 
                        new compressed_fragment_t());
            }
        }
    }

    packet_t *packet_top() const {
        assert(packet_count() > 0);
        packet_t *packet 
            = reinterpret_cast<packet_t *>(output_packets->data);
        return packet;
    }

    void packet_pop() {
        list_pop(output_packets);
    }

    int packet_count() const {
        return list_length(output_packets);
    }

private:
    void add_fragment_to_pending(compressed_fragment_t *fragment) {
        assert(fragment->is_complete());
        list_push(compressed_pending_fragments, fragment);
        if (fragment->is_last()) {
            flush_compressed_pending_fragments_to_pending_fragments();
        }
    }

    void flush_compressed_pending_fragments_to_pending_fragments() {
        fragment_t *result = NULL;
        list_reverse(compressed_pending_fragments);
        for (LIST *m = compressed_pending_fragments; m; m = list_rest(m)) {
            compressed_fragment_t *fragment 
                = reinterpret_cast<compressed_fragment_t *>(m->data);

            char *pointer = fragment->get_pointer();
            size_t uncompressed_size = fragment->get_uncompressed_size();
            while (uncompressed_size) {
                if (!result) { result = new fragment_t(); }
                char *new_pointer = result->append(pointer, uncompressed_size);
                uncompressed_size -= new_pointer - pointer;
                pointer = new_pointer;
                if (result->is_complete()) {
                    add_fragment_to_output(result);
                    result = NULL;
                }
            }
        }
        list_free(compressed_pending_fragments, 0);
        compressed_pending_fragments = NULL;
    }

    void add_fragment_to_output(fragment_t *fragment) {
        assert(fragment->is_complete());
        list_push(pending_fragments, fragment);
        if (fragment->is_last()) {
            flush_pending_fragments_to_output();
        }
    }

    void flush_pending_fragments_to_output() {
        packet_t *packet = new packet_t();
        list_reverse(pending_fragments);
        for (LIST *m = pending_fragments; m; m = list_rest(m)) {
            fragment_t *fragment 
                = reinterpret_cast<fragment_t *>(m->data);
            packet->append(fragment);
        }
        list_push(output_packets, packet);
        list_free(pending_fragments, 0);
        pending_fragments = NULL;
    }

private:
    LIST *output_packets;
    LIST *pending_fragments;
    LIST *compressed_pending_fragments;
};


typedef struct acceptor_s {
    int fd;
    struct event event;
} acceptor_t;

typedef struct connection_s {
    int fd;
    struct event event;
    Vio *vio;
    int state;
    bool protocol41;
    bool compress;
    bool ssl;
    packet_input_stream *input_stream;
    packet_output_stream *output_stream;
} connection_t;

typedef struct connector_s {
    connection_t ccon;
    connection_t scon;
} connector_t;

void set_nonblockable(int fd) {
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | O_NONBLOCK);
}

int create_listener_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return fd;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return -1;
    }

    if (listen(fd, 127) == -1) {
        return -1;
    }

    set_nonblockable(fd);

    const int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&one, sizeof(one)) == -1) {
        return -1;
    }
    
    return fd;
}

int connect_to_peer(const char *vhost, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return fd;
    }
    struct sockaddr_in saddr;
    memset((void *)&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(vhost);
    if (connect(fd, (const struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        return -1;
    }
    return fd;
}

static void
regenerate_acceptor_event(acceptor_t *acceptor) {
    event_add(&acceptor->event, NULL);
}

static void
regenerate_connector_event(connector_t *connector) {
    event_add(&connector->ccon.event, NULL);
    event_add(&connector->scon.event, NULL);
}

static void
finalize_connector(connector_t *connector) {
    event_del(&connector->ccon.event);
    event_del(&connector->scon.event);
    close(connector->ccon.fd);
    close(connector->scon.fd);
    free((void*)connector);
}

static int
write_nbytes_to_connection(Vio *vio, char *buf, size_t size) {
    size_t left = size;
    while (left) {
        int written_bytes = vio_write(fd, buf, left);
        if (written_bytes <= 0) {
            if (written_bytes < 0 && (errno == EINTR || errno == EAGAIN))
                continue;
            return 1;
        }
        buf += written_bytes;
        left -= written_bytes;
    }
    return 0;
}

static void
drain_output_stream(connection_t *conn) {
    packet_output_stream *output_stream = conn->output_stream; 
    if (output_stream->packet_count()) {
        char *data; size_t write_len;
        output_stream->get_packets(&data, &write_len);
        write_nbytes_to_connection(conn->vio, data, write_len);
        output_stream->reset_packets();
    }
}

/* mysql protocol types 
 * TODO: bounds checking and leaks
*/
struct mysql_protocol_itemtype {
    virtual char *pack(char *p) = 0;
    virtual char *unpack(char *p) = 0;
};
struct fixed_integer1: public mysql_protocol_itemtype {
    char *pack(char *p) { *p++ = b; return p + 1; }
    char *unpack(char *p) { b = p[0]; return p + 1;}
    uchar b;
};
struct fixed_integer2: public mysql_protocol_itemtype {
    char *pack(char *p) { int2store(p, b); return p + 2; }
    char *unpack(char *p) { b = uint2korr(p); return p + 2;}
    uint b;
};
struct fixed_integer3: public mysql_protocol_itemtype {
    char *pack(char *p) { int3store(p, b); return p + 3; }
    char *unpack(char *p) { b = uint3korr(p); return p + 3;}
    uint b;
};
struct fixed_integer4: public mysql_protocol_itemtype {
    char *pack(char *p) { int4store(p, b); return p + 4; }
    char *unpack(char *p) { b = uint4korr(p); return p + 4;}
    uint b;
};
struct fixed_integer6: public mysql_protocol_itemtype {
    char *pack(char *p) { int6store(p, b); return p + 6; }
    char *unpack(char *p) { b = uint6korr(p); return p + 6;}
    ulonglong b;
};
struct fixed_integer8: public mysql_protocol_itemtype {
    char *pack(char *p) { int8store(p, b); return p + 8; }
    char *unpack(char *p) { b = uint8korr(p); return p + 8;}
    ulonglong b;
};

template <int n>
struct fixedlen_string: public mysql_protocol_itemtype {
    char *pack(char *p) { memcpy(p, b, n); return p + n; }
    char *unpack(char *p) { memcpy(b, p, n); return p + n;}
    uchar b[n];
};

struct nullterminated_string: public mysql_protocol_itemtype {
    char *pack(char *p) { 
        int len = strlen(p);
        memcpy(p, b, len);
        return p + len;
    }
    char *unpack(char *p) { 
        int len = strlen(p);
        b = (char*)realloc(len);
        return p + len;
    }
    char *b;
};

struct mysql_protocol_handshake_packet: public mysql_protocol_itemtype {
    char *pack(char *p);
    char *unpack(char *p);

    fixed_integer1 proto_version; 
    nullterminated_string server_version;
    fixed_integer4 connection_id;
    fixedlen_string<8> auth_plugin_data_part1;
    fixed_integer1 filler;
    fixed_integer2 capability_flags;
};

struct mysql_protocol_ssl_request_packet: public mysql_protocol_itemtype {
    char *pack(char *p);
    char *unpack(char *p);

    fixed_integer4 capability_flags; 
};

struct mysql_protocol_handshake_response_packet: public mysql_protocol_itemtype {
    char *pack(char *p);
    char *unpack(char *p);

    fixed_integer4 capability_flags; 
    fixed_integer4 max_packet_size; 
    fixed_integer1 character_set; 
};

struct mysql_protocol_ok_packet: public mysql_protocol_itemtype {
    char *pack(char *p);
    char *unpack(char *p);
    uchar b;
};

struct mysql_protocol_err_packet: public mysql_protocol_itemtype {
    char *pack(char *p);
    char *unpack(char *p);
    uchar b;
};

struct mysql_protocol_eof_packet: public mysql_protocol_itemtype {
    char *pack(char *p);
    char *unpack(char *p);
    uchar b;
};

/* return true if connection is still need to keep alive,
 * otherwise stop it */
static bool
client_connector_processor(connector_t *connector, packet_t *packet) {
    connection_t *sconn = connector->scon;
    connection_t *cconn = connector->scon;
    switch (cconn->state) {
    case CLIENT_NONE: {
        break;
    }
    case AWAIT_INITIAL_HANDSHAKE_FROM_SERVER: {
        /* determine ssl_connection_request or handshake_response_packet */
        bool is_ssl_handshake_request = packet->get_size() == 4 + 4 + 1 + 23;
        if (is_ssl_handshake_request) {
            mysql_protocol_ssl_request_packet ssl_request;
            ssl_request.unpack(packet->get_pointer());
            cconn->protocol41 = ssl_request.capability_flags.b & CLIENT_PROTOCOL41;
            cconn->compress = ssl_request.capability_flags.b & CLIENT_COMPRESS;
            cconn->ssl = ssl_request.capability_flags.b & CLIENT_SSL;

            /* drain immediately, cause switching to ssl */
            sconn->output_stream->feed_packet(packet);
            drain_output_stream(sconn); /

            /* switch to ssl */
            vio_reset(sconn->vio, VIO_TYPE_SSL, sconn->fd, 0, 1);
            vio_reset(cconn->vio, VIO_TYPE_SSL, cconn->fd, 0, 1);

            cconn->state = SSL_CONNECTION_REQUEST;
            return;
        }
        /* else handshake_response is there, just fall through */
    }
    case SSL_CONNECTION_REQUEST:
    case HANDSHAKE_RESPONSE_PACKET: {
        /* do not initialize twice */
        if (cconn->state != SSL_CONNECTION_REQUEST) {
            mysql_protocol_handshake_response_packet response;
            response.unpack(packet->get_pointer());
            cconn->protocol41 = response.capability_flags.b & CLIENT_PROTOCOL41;
            cconn->compress = response.capability_flags.b & CLIENT_COMPRESS;
            cconn->ssl = response.capability_flags.b & CLIENT_SSL;
        }
        cconn->state = STATUS_PACKET_FROM_SERVER;
        sconn->state = AWAIT_STATUS_PACKET_FROM_SERVER;
        sconn->output_stream->feed_packet(packet);
        break;
    }
    case STATUS_PACKET_FROM_SERVER: {
        break;
    }
    case CLIENT_COMMAND_PHASE: {
        sconn->output_stream->feed_packet(packet);
        break;
    }
    };
}

static bool
server_connector_processor(connector_t *connector, packet_t *packet) {
    connection_t *sconn = connector->scon;
    connection_t *cconn = connector->scon;
    switch (sconn->state) {
    case SERVER_NONE: {
        sconn->state = INITIAL_HANDSHAKE_FROM_SERVER;
        cconn->state = AWAIT_INITIAL_HANDSHAKE_FROM_SERVER;

        mysql_protocol_handshake_packet initial_handshake;
        initial_handshake.unpack(packet->get_pointer());

        sconn->protocol41 = initial_handshake.capability_flags.b 
            & CLIENT_PROTOCOL_41;
        sconn->ssl = initial_handshake.capability_flags.b & CLIENT_SSL;
        sconn->compress = initial_handshake.capability_flags.b & CLIENT_COMPRESS;

        cconn->output_stream->feed_packet(packet);
        break;
    }
    case INITIAL_HANDSHAKE_FROM_SERVER: {
        break;
    }
    case AWAIT_SSL_CONNECTION_REQUEST: {
        break;
    }
    case AWAIT_HANDSHAKE_RESPONSE_PACKET: {
        break;
    }
    case AWAIT_STATUS_PACKET_FROM_SERVER: {
        /* is err-packet */
        if (packet->get_pointer()[0] == 0xff) {
            return;
        } else {
            /* ok-packet arrived, switch to command-phase */
            sconn->state = SERVER_COMMAND_PHASE;
            cconn->state = CLIENT_COMMAND_PHASE;

            /* drain immediately, cause turning on compression */
            cconn->output_stream->feed_packet(packet);
            drain_output_stream(cconn); /

            /* turning on compression if needed */
            if (sconn->compress && cconn->compress) {
                sconn->input_stream = new mysql_packet_input_stream_compressed();
                sconn->output_stream = new mysql_packet_output_stream_compressed();
                cconn->input_stream = new mysql_packet_input_stream_compressed();
                cconn->output_stream = new mysql_packet_output_stream_compressed();
            }
        }
        break;
    }
    case SERVER_COMMAND_PHASE: {
        cconn->output_stream->feed_packet(packet);
        break;
    }
    };
}

static void
server_connector_handler(int fd, short event, void *arg) {
    size_t was_read; uchar buf[1024];
    connector_t *connector = (connector_t *)arg;

    was_read = vio_read(connector->scon.vio, buf, sizeof(buf));
    if (was_read == (size_t)-1 || !was_read) { 
        if (was_read == (size_t)-1 && (errno == EAGAIN || errno == EINTR)) {
            event_add(&connector->scon.event, NULL);
        } else {
            finalize_connector(connector);
        }
        return;
    }

    connector->scon.input_stream->feed_data(buf, was_read);
    while (connector->scon.input_stream->packet_count()) {
        server_connector_processor(connector, 
                connector->scon.input_stream->packet_top());
        connector->scon.input_stream->packet_pop();
    }
    event_add(&connector->scon.event, NULL);
    drain_output_stream(connector->scon);
}

static void
client_connector_handler(int fd, short event, void *arg) {
    size_t was_read; uchar buf[1024];
    connector_t *connector = (connector_t *)arg;

    was_read = vio_read(connector->ccon.vio, buf, sizeof(buf));
    if (was_read == (size_t)-1 || !was_read) { 
        if (was_read == (size_t)-1 && errno == EAGAIN) {
            event_add(&connector->ccon.event, NULL);
        } else {
            finalize_connector(connector);
        }
        return;
    }

    connector->ccon.input_stream->feed_data(buf, was_read);
    while (connector->ccon.input_stream->packet_count()) {
        client_connector_processor(connector,
                connector->ccon.input_stream->packet_top());
        connector->ccon.input_stream->packet_pop();
    }
    event_add(&connector->ccon.event, NULL);
    drain_output_stream(connector->ccon);
}

static void 
acceptor_handler(int fd, short event, void *arg) {
    acceptor_t *acceptor = (acceptor_t *)arg;
    int peer_fd = accept(acceptor->fd, NULL, NULL);
    if (peer_fd < 0) {
        return;
    }

    connector_t *connector = (connector_t *)malloc(sizeof(connector_t));
    memset((void *)connector, 0, sizeof(connector_t));
    set_nonblockable(peer_fd);
    connector->ccon.fd = peer_fd;

    int server_fd;
    if ((server_fd = connect_to_peer(k_mysql_server, k_mysql_server_port)) >= 0) {
        set_nonblockable(server_fd);
        connector->scon.fd = server_fd; 

        /* flags=1 localhost here */
        connector->ccon.vio = vio_new(connector->ccon.fd, VIO_TYPE_TCPIP, 1);
        connector->scon.vio = vio_new(connector->scon.fd, VIO_TYPE_TCPIP, 1);

        connector->scon.input_stream = new mysql_packet_input_stream();
        connector->scon.output_stream = new mysql_packet_output_stream();

        connector->ccon.input_stream = new mysql_packet_input_stream();
        connector->ccon.output_stream = new mysql_packet_output_stream();

        connector->ccon.state = CLIENT_NONE;
        connector->scon.state = SERVER_NONE;

        event_set(&connector->ccon.event, connector->ccon.fd, 
            EV_READ, client_connector_handler, (void *)connector);
        event_set(&connector->scon.event, connector->scon.fd, 
            EV_READ, server_connector_handler, (void *)connector);
        regenerate_connector_event(connector);
    } else {
        close(peer_fd);
        free(connector);
    }
    regenerate_acceptor_event(acceptor);
}

int
initialize_acceptor(acceptor_t *acceptor, int port) {
    acceptor->fd = create_listener_socket(port);
    if (acceptor->fd == -1) {
        return -1;
    }
    event_set(&acceptor->event, acceptor->fd, 
                EV_READ | EV_PERSIST, acceptor_handler, (void *)acceptor);
    event_add(&acceptor->event, NULL);
    return acceptor->fd;
}

int main(int argc, char **argv) {
	MY_INIT(argv[0]);

    event_init();

    acceptor_t acceptor;
    if (initialize_acceptor(&acceptor, k_proxy_listen_port) < 0) {
        return 1;
    }

    event_dispatch();
    return 0;
}
