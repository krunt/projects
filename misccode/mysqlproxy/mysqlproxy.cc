
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "mysqlcommon.h"
#include "mysqlvio.h"

#include <event.h>
#include <string>
#include <deque>
#include <vector>
#include <fstream>

/* mysql-commands types */
enum enum_server_command
{
  COM_SLEEP, COM_QUIT, COM_INIT_DB, COM_QUERY, COM_FIELD_LIST,
  COM_CREATE_DB, COM_DROP_DB, COM_REFRESH, COM_SHUTDOWN, COM_STATISTICS,
  COM_PROCESS_INFO, COM_CONNECT, COM_PROCESS_KILL, COM_DEBUG, COM_PING,
  COM_TIME, COM_DELAYED_INSERT, COM_CHANGE_USER, COM_BINLOG_DUMP,
  COM_TABLE_DUMP, COM_CONNECT_OUT, COM_REGISTER_SLAVE,
  COM_STMT_PREPARE, COM_STMT_EXECUTE, COM_STMT_SEND_LONG_DATA, COM_STMT_CLOSE,
  COM_STMT_RESET, COM_SET_OPTION, COM_STMT_FETCH, COM_DAEMON,
  /* Must be last */
  COM_END
};

static const char *server_command_str[] = {
  "SLEEP", "QUIT", "INIT_DB", "QUERY", "FIELD_LIST", "CREATE_DB", "DROP_DB",
  "REFRESH", "SHUTDOWN", "STATISTICS", "PROCESS_INFO", "CONNECT", "PROCESS_KILL",
  "DEBUG", "PING", "TIME", "DELAYED_INSERT", "CHANGE_USER", "BINLOG_DUMP",
  "TABLE_DUMP", "CONNECT_OUT", "REGISTER_SLAVE", "STMT_PREPARE", "STMT_EXECUTE",
  "STMT_SEND_LONG_DATA", "STMT_CLOSE", "STMT_RESET", "SET_OPTION",
  "STMT_FETCH", "DAEMON", NULL,
};

const int MAX_PACKET_LENGTH = 256L*256L*256L-1;

namespace {
template <int step = -1>
class hexdumper {
    static char hexcodes[16+1];
public:
    hexdumper(const std::string &logname)
        : fs(logname.c_str(), std::ios_base::out),
          decoded(0)
    {}

    void flush() {
        fs.flush();
    }

    void write(char *buf, int size) {
        for (int i = 0; i < size; ++i) {
            fs.put(hexcodes[(buf[i] & 0xF0) >> 4]);
            fs.put(hexcodes[buf[i] & 0x0F]);
            //fs.put(' ');

            if (step == -1) {
                fs.put(' ');
                decoded++;
            } else {
                fs.put(++decoded % step == 0 ? '\n' : ' ');
            }
        }
    }

    int get_decoded() const { 
        return decoded; 
    }

private:
    std::fstream fs;
    int decoded;
};

template <int x>
char hexdumper<x>::hexcodes[16+1] = "0123456789abcdef";

struct fragment_t {
    fragment_t():
        fragment_size(0), fragment_seq_id(0),
        size_determined(false),
        pending_size(0)
    {}

    ~fragment_t() {}

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

        if (fragment_data.length() + size < fragment_size) {
            fragment_data.append(data, size);
            return data + size;
        } else {
            size_t bytes_to_fill = fragment_size - fragment_data.length();
            fragment_data.append(data, bytes_to_fill);
            return data + bytes_to_fill;
        }
    }

    char *get_pointer() const {
        return (char*)fragment_data.data();
    }

    size_t get_size() const {
        return fragment_size;
    }

    bool is_complete() const {
        return size_determined && fragment_size == fragment_data.length();
    }

    bool is_last() const {
        return size_determined && fragment_size != MAX_PACKET_LENGTH;
    }

private:
    size_t fragment_size;
    uchar fragment_seq_id;
    std::string fragment_data;

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
    {}

    ~compressed_fragment_t() {}

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

        if (fragment_data.length() + size < compressed_fragment_size) {
            fragment_data.append(data, size);
            return data + size;
        } else {
            size_t bytes_to_fill = compressed_fragment_size 
                - fragment_data.length();
            fragment_data.append(data, bytes_to_fill);
            finish_fragment();
            return data + bytes_to_fill;
        }
    }

    char *get_pointer() const {
        return (char*)fragment_data.data();
    }

    size_t get_compressed_size() const {
        return compressed_fragment_size;
    }

    size_t get_uncompressed_size() const {
        return uncompressed_fragment_size;
    }

    bool is_complete() const {
        return size_determined && completed;
    }

    bool is_last() const {
        return size_determined && get_uncompressed_size() != MAX_PACKET_LENGTH - 4;
    }

private:
    void finish_fragment() {
        /* if wasn't compressed */
        if (uncompressed_fragment_size) {
            fragment_data.reserve(uncompressed_fragment_size);
            my_uncompress((uchar*)fragment_data.data(), compressed_fragment_size,
                &uncompressed_fragment_size); 
        } else {
            uncompressed_fragment_size = compressed_fragment_size;
        }
        completed = true;
    }

    size_t compressed_fragment_size;
    uchar fragment_seq_id;
    size_t uncompressed_fragment_size;
    bool completed;
    std::string fragment_data;

    bool size_determined;
    size_t pending_size;
    char pending_buf[7];
};

struct packet_t {
    packet_t()
    {}

    ~packet_t() 
    {}

    void append(fragment_t *fragment) {
        packet_data.append(fragment->get_pointer(), fragment->get_size());
    }

    char *get_pointer() const {
        return (char*)packet_data.data();
    }

    size_t get_size() const {
        return packet_data.length();
    }

private:
    std::string packet_data;
};
} /* namespace */

/* constants */
const char *k_mysql_server = "127.0.0.1";
const int k_proxy_listen_port = 19999;
const int k_mysql_server_port = 40000;

const char *client_key = "certsdir/client-key.pem";
const char *client_cert = "certsdir/client-cert.pem";
const char *client_ca_file = "certsdir/ca-cert.pem";

const char *server_key = "certsdir/server-key.pem";
const char *server_cert = "certsdir/server-cert.pem";
const char *server_ca_file = "certsdir/ca-cert.pem";

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

class mysql_packet_seqid {
public:
    mysql_packet_seqid()
    { reset(); }

    int get() const { return seqid; }
    int operator++() { seqid++; return seqid; }
    int operator++(int) { int tmp = seqid; this->operator++(); return tmp; }
    void reset() { seqid = 0; }

private:
    int seqid;
};

class packet_output_stream {
public:
    packet_output_stream(mysql_packet_seqid *aseqid, 
            mysql_packet_seqid *acompressedseqid = NULL) 
        : seqid(aseqid), compressed_seqid(acompressedseqid)
    {}

    virtual void feed_packet(uchar *data, size_t len) = 0;
    virtual void reset_packets() = 0;
    virtual void get_packets(char **packet_begin, size_t *write_size) = 0;
    virtual int  packet_count() const = 0;

    void feed_packet(packet_t *packet) { 
        feed_packet((uchar*)packet->get_pointer(), packet->get_size());    
    }

    void reset_seqid() { 
        seqid->reset(); 
        if (compressed_seqid) {
            compressed_seqid->reset(); 
        }
    }

protected:
    mysql_packet_seqid *seqid;
    mysql_packet_seqid *compressed_seqid;

#ifdef DEBUG
public:
    std::vector<std::pair<int, int> > output_packet_offsets;
#endif

};

class mysql_packet_handler {
public:
    virtual void handle(packet_t *packet) = 0;
};

class mysql_write_handler: public mysql_packet_handler {
public:
    mysql_write_handler(packet_output_stream *s)
        : output_stream(s)
    {}

    void handle(packet_t *packet) {
        output_stream->feed_packet(packet);
    }

private:
    packet_output_stream *output_stream;
};

class mysql_statistics_handler: public mysql_packet_handler {
public:
    struct statistics {
        statistics() { memset(command_counters, 0, COM_END); }
        int command_counters[COM_END];
    };

public:
    mysql_statistics_handler()
    {}

    void handle(packet_t *packet) {
        int command_type = packet->get_pointer()[0];
        if (command_type < COM_END) {
            ++stat.command_counters[command_type];
        }
    }

    void print_statistics() const {
        for (int i = 0; i < COM_END; ++i) {
            fprintf(stderr, "%s: %d,", 
                server_command_str[i], stat.command_counters[i]);
        }
        fprintf(stderr, "\n");
    }

private:
    statistics stat;
};

class mysql_packet_output_stream: public packet_output_stream {
public:
    mysql_packet_output_stream(mysql_packet_seqid *aseqid) 
        : packet_output_stream(aseqid),
          output_packet_count(0), 
          output_packet_fragments_count(0)
    {}

    ~mysql_packet_output_stream() 
    {}

    void feed_packet(uchar *data, size_t len) {
        uchar packet_header[3+1];
        while (len >= MAX_PACKET_LENGTH) {
            int3store(packet_header, MAX_PACKET_LENGTH);
            packet_header[3] = (uchar)seqid->get();
#ifdef DEBUG
            output_packet_offsets.push_back(std::make_pair(
                        output.size(), MAX_PACKET_LENGTH));
#endif
            output.append((char*)packet_header, sizeof(packet_header));
            output.append((char*)data, MAX_PACKET_LENGTH);

            data += MAX_PACKET_LENGTH;
            len -= MAX_PACKET_LENGTH;

            output_packet_fragments_count++;
        }

        int3store(packet_header, len);
        packet_header[3] = (uchar)(*seqid)++;

#ifdef DEBUG
        output_packet_offsets.push_back(std::make_pair(output.size(), len));
#endif

        output.append((char*)packet_header, sizeof(packet_header));
        output.append((char*)data, len);

        output_packet_fragments_count++;
        output_packet_count++;
    }

    void reset_packets() {
        output.clear();
        output_packet_count = 0;
        output_packet_fragments_count = 0;
#ifdef DEBUG
        output_packet_offsets.clear();
#endif
    }

    void get_packets(char **packet_begin, size_t *write_size) {
        *packet_begin = (char*)output.data();
        *write_size = output.length();
    }

    int  packet_count() const {
        return output_packet_count;
    }

private:
    std::string output;
    int output_packet_count;
    int output_packet_fragments_count;
};

class mysql_packet_output_stream_compressed: public packet_output_stream {
public:
    mysql_packet_output_stream_compressed(mysql_packet_seqid *aseqid, 
            mysql_packet_seqid *acompressedseqid) 
        : packet_output_stream(aseqid, acompressedseqid),
          output_packet_count(0), 
          output_packet_fragments_count(0)
    {}

    ~mysql_packet_output_stream_compressed()
    {}

    void feed_packet(uchar *data, size_t len) {
        while (len >= MAX_PACKET_LENGTH) {
            size_t uncompressed_size = 3+1+MAX_PACKET_LENGTH;
            uchar *uncompressed_packet_data 
                = (uchar*)malloc(uncompressed_size);
            int3store(uncompressed_packet_data, MAX_PACKET_LENGTH);
            uncompressed_packet_data[3] = (uchar)seqid->get();
            memcpy(uncompressed_packet_data + 3 + 1, data, MAX_PACKET_LENGTH);
            append_packet_to_output(uncompressed_packet_data, uncompressed_size);
            free(uncompressed_packet_data);

            data += MAX_PACKET_LENGTH;
            len -= MAX_PACKET_LENGTH;
        }

        size_t uncompressed_size = 3+1+len;
        uchar *uncompressed_packet_data 
            = (uchar*)malloc(uncompressed_size);
        int3store(uncompressed_packet_data, len);
        uncompressed_packet_data[3] = (uchar)(*seqid)++;
        memcpy(uncompressed_packet_data + 3 + 1, data, len);
        append_packet_to_output(uncompressed_packet_data, uncompressed_size);
        free(uncompressed_packet_data);
    }

    void reset_packets() {
        output.clear();
        output_packet_count = 0;
        output_packet_fragments_count = 0;
    }

    void get_packets(char **packet_begin, size_t *write_size) {
        *packet_begin = (char*)output.data();
        *write_size = output.length();
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
            compressed_packet_header[3] = (uchar)compressed_seqid->get();
            int3store(compressed_packet_header+3+1, uncompressed_packet_original);

            output.append((char*)compressed_packet_header, 
                sizeof(compressed_packet_header));
            output.append((char*)uncompressed_packet_data, 
                    uncompressed_packet_size);
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
        compressed_packet_header[3] = (uchar)(*compressed_seqid)++;
        int3store(compressed_packet_header+3+1, uncompressed_packet_original);

        output.append((char*)compressed_packet_header, 
            sizeof(compressed_packet_header));
        output.append((char*)uncompressed_packet_data, uncompressed_packet_size);

        output_packet_fragments_count++;
        output_packet_count++;
    }

private:
    std::string output;
    int output_packet_count;
    int output_packet_fragments_count;
};


class packet_input_stream {
public:
    virtual void feed_data(uchar *data, size_t len) = 0;
    virtual packet_t *packet_top() const = 0;
    virtual void packet_pop() = 0;
    virtual int  packet_count() const = 0;
};

/* TODO: leaking memory when freeing list */
class mysql_packet_input_stream: public packet_input_stream  {
public:
    mysql_packet_input_stream() 
    {}

    ~mysql_packet_input_stream() 
    {}

    void feed_data(uchar *data, size_t len) {
        if (pending_fragments.empty()) {
            pending_fragments.push_back(new fragment_t());
        }

        while (len) {
            fragment_t *fragment = pending_fragments.back();

            uchar *newdata = (uchar*)fragment->append((char*)data, len);
            len -= newdata - data;
            data = newdata;

            if (fragment->is_complete()) {
                /* skip empty packets */
                if (!fragment->get_size()) {
                    pending_fragments.pop_back();
                    continue;
                }
                add_fragment_to_output(fragment);
                pending_fragments.push_back(new fragment_t());
            }
        }
    }

    packet_t *packet_top() const {
        assert(packet_count() > 0);
        return output_packets[0];
    }

    void packet_pop() {
        assert(packet_count() > 0);
        output_packets.pop_front();
    }

    int packet_count() const {
        return output_packets.size();
    }

private:
    void add_fragment_to_output(fragment_t *fragment) {
        assert(fragment->is_complete());
        if (fragment->is_last()) {
            flush_pending_fragments_to_output();
        }
    }

    void flush_pending_fragments_to_output() {
        packet_t *packet = new packet_t();
        for (int i = 0; i < pending_fragments.size(); ++i) {
            packet->append(pending_fragments[i]);
        }
        output_packets.push_back(packet);
        pending_fragments.clear();
    }

private:
    std::deque<packet_t *> output_packets;
    std::deque<fragment_t *> pending_fragments;
};


class mysql_packet_input_stream_compressed: public packet_input_stream {
public:
    mysql_packet_input_stream_compressed() 
    {}

    ~mysql_packet_input_stream_compressed() 
    {}

    void feed_data(uchar *data, size_t len) {
        if (compressed_pending_fragments.empty()) {
            compressed_pending_fragments.push_back(new compressed_fragment_t());
        }

        while (len) {
            compressed_fragment_t *fragment = compressed_pending_fragments.back();

            uchar *newdata = (uchar*)fragment->append((char*)data, len);
            len -= newdata - data;
            data = newdata;

            if (fragment->is_complete()) {
                add_fragment_to_pending(fragment);
                compressed_pending_fragments.push_back(new compressed_fragment_t());
            }
        }
    }

    packet_t *packet_top() const {
        assert(packet_count() > 0);
        return output_packets[0];
    }

    void packet_pop() {
        assert(packet_count() > 0);
        output_packets.pop_front();
    }

    int packet_count() const {
        return output_packets.size();
    }

private:
    void add_fragment_to_pending(compressed_fragment_t *fragment) {
        assert(fragment->is_complete());
        if (fragment->is_last()) {
            flush_compressed_pending_fragments_to_pending_fragments();
        }
    }

    void flush_compressed_pending_fragments_to_pending_fragments() {
        for (int i = 0; i < compressed_pending_fragments.size(); ++i) {
            compressed_fragment_t *fragment = compressed_pending_fragments[i];

            char *pointer = fragment->get_pointer();
            size_t uncompressed_size = fragment->get_uncompressed_size();
            while (uncompressed_size) {
                if (pending_fragments.empty()) {
                    pending_fragments.push_back(new fragment_t());
                }

                fragment_t *result = pending_fragments.back();
                char *new_pointer = result->append(pointer, uncompressed_size);
                uncompressed_size -= new_pointer - pointer;
                pointer = new_pointer;
                if (result->is_complete()) {
                    add_fragment_to_output(result);
                }
            }
        }
        compressed_pending_fragments.clear();
    }

    void add_fragment_to_output(fragment_t *fragment) {
        assert(fragment->is_complete());
        if (fragment->is_last()) {
            flush_pending_fragments_to_output();
        } else {
            pending_fragments.push_back(new fragment_t());
        }
    }

    void flush_pending_fragments_to_output() {
        packet_t *packet = new packet_t();
        for (int i = 0; i < pending_fragments.size(); ++i) {
            fragment_t *fragment = pending_fragments[i];
            packet->append(fragment);
        }
        output_packets.push_back(packet);
        pending_fragments.clear();
    }

private:
    std::deque<packet_t *> output_packets;
    std::deque<fragment_t *> pending_fragments;
    std::deque<compressed_fragment_t *> compressed_pending_fragments;
};

struct connection_t;
static void drain_output_stream(connection_t *conn);

struct acceptor_t {
    int fd;
    struct event event;
};

struct connection_t {
    connection_t():
        fd(0), vio(NULL), state(0),
        protocol41(false), compress(false),
        ssl(false), compression_was_switched(false),
        input_stream(NULL),
        output_stream(NULL)
#ifdef HAVE_OPENSSL
        ,ssl_connector(NULL)
#endif
    {}

    void handle(packet_t *packet) {
        for (int i = 0; i < packet_handlers.size(); ++i) {
            packet_handlers[i]->handle(packet);
        }
    }

    int fd;
    struct event event;
    Vio *vio;
    int state;
    bool protocol41;
    bool compress;
    bool ssl;
    bool compression_was_switched;

    packet_input_stream *input_stream;
    packet_output_stream *output_stream;

    std::vector<mysql_packet_handler *> packet_handlers;

#ifdef DEBUG
    hexdumper<> *dumper;
    hexdumper<> *packet_dumper;
    hexdumper<8> *packet_offset_dumper;
#endif

#ifdef HAVE_OPENSSL
    struct st_VioSSLFd *ssl_connector;
#endif

};

struct connector_t {
    connection_t ccon;
    connection_t scon;
};

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

    const int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&one, sizeof(one)) == -1) {
        return -1;
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return -1;
    }

    if (listen(fd, 127) == -1) {
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
#ifdef VERBOSE
    if (connector->ccon.packet_handlers.size() > 1) {
    dynamic_cast<mysql_statistics_handler *>(connector->ccon.packet_handlers[0])
        ->print_statistics();
    }
#endif
    event_del(&connector->ccon.event);
    event_del(&connector->scon.event);
    close(connector->ccon.fd);
    close(connector->scon.fd);
    delete connector;
}

static int
write_nbytes_to_connection(Vio *vio, char *buf, size_t size) {
    size_t left = size;
    while (left) {
        int written_bytes = vio_write(vio, (const uchar*)buf, left);
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

void
drain_output_stream(connection_t *conn) {
    packet_output_stream *output_stream = conn->output_stream; 
    if (output_stream->packet_count()) {
        char *data; size_t write_len; int res;
        output_stream->get_packets(&data, &write_len);
        res = write_nbytes_to_connection(conn->vio, data, write_len);

#ifdef DEBUG
        fprintf(stderr, "write: %d\n", res);

        if (conn->state == CLIENT_COMMAND_PHASE
                || conn->state == SERVER_COMMAND_PHASE)
        {
            int offset = conn->packet_dumper->get_decoded();

            conn->packet_dumper->write(data, write_len);
            conn->packet_dumper->flush();

            const std::vector<std::pair<int, int> > &offsets 
                = output_stream->output_packet_offsets;
            for (int i = 0; i < offsets.size(); ++i) {
                int final_offset = offset + offsets[i].first;
                conn->packet_offset_dumper->write((char*)&final_offset, 4);
                conn->packet_offset_dumper->write((char*)&offsets[i].second, 4);
                conn->packet_offset_dumper->flush();
            }

        }
#endif
        output_stream->reset_packets();
    }
}

/* mysql protocol types 
 * TODO: bounds checking and leaks
*/
struct mysql_protocol_itemtype {
    virtual char *pack(char *p, char *end) = 0;
    virtual char *unpack(char *p, char *end) = 0;
};
struct fixed_integer1: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { *p++ = b; return p + 1; }
    char *unpack(char *p, char *end) { b = p[0]; return p + 1;}
    uchar b;
};
struct fixed_integer2: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { int2store(p, b); return p + 2; }
    char *unpack(char *p, char *end) { b = uint2korr(p); return p + 2;}
    uint b;
};
struct fixed_integer3: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { int3store(p, b); return p + 3; }
    char *unpack(char *p, char *end) { b = uint3korr(p); return p + 3;}
    uint b;
};
struct fixed_integer4: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { int4store(p, b); return p + 4; }
    char *unpack(char *p, char *end) { b = uint4korr(p); return p + 4;}
    uint b;
};
struct fixed_integer6: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { int6store(p, b); return p + 6; }
    char *unpack(char *p, char *end) { b = uint6korr(p); return p + 6;}
    ulonglong b;
};
struct fixed_integer8: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { int8store(p, b); return p + 8; }
    char *unpack(char *p, char *end) { b = uint8korr(p); return p + 8;}
    ulonglong b;
};

template <int n>
struct fixedlen_string: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { memcpy(p, b, n); return p + n; }
    char *unpack(char *p, char *end) { memcpy(b, p, n); return p + n; }
    uchar b[n];
};

struct nullterminated_string: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { 
        int len = strlen(p);
        memcpy(p, b, len);
        return p + len + 1;
    }
    char *unpack(char *p, char *end) { 
        int len = strlen(p) + 1;
        b = (char*)realloc(b, len);
        memcpy(b, p, len);
        b[len] = 0;
        return p + len;
    }
    char *b;
};

struct eof_string: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { 
        memcpy(p, b, size);
        return p + size;
    }
    char *unpack(char *p, char *end) { 
        size = end - p;
        b = (char*)realloc(b, size);
        memcpy(b, p, size);
        return p + size;
    }
    char *b;
    size_t size;
};

struct mysql_protocol_handshake_packet: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) {
        return capability_flags.pack(
        filler.pack(
        auth_plugin_data_part1.pack(
        connection_id.pack(
        server_version.pack(
        proto_version.pack(p, end), end), end), end), end), end);
    }
    char *unpack(char *p, char *end) {
        return capability_flags.unpack(
        filler.unpack(
        auth_plugin_data_part1.unpack(
        connection_id.unpack(
        server_version.unpack(
        proto_version.unpack(p, end), end), end), end), end), end);
    }

    fixed_integer1 proto_version; 
    nullterminated_string server_version;
    fixed_integer4 connection_id;
    fixed_integer8 auth_plugin_data_part1;
    fixed_integer1 filler;
    fixed_integer2 capability_flags;
};

struct mysql_protocol_ssl_request_packet: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { return capability_flags.pack(p, end); }
    char *unpack(char *p, char *end) { return capability_flags.unpack(p, end); }
    fixed_integer4 capability_flags; 
};

struct mysql_protocol_handshake_response_packet: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) {
        return character_set.pack(max_packet_size.pack(
            capability_flags.pack(p, end), end), end);
    }
    char *unpack(char *p, char *end) {
        return character_set.unpack(max_packet_size.unpack(
            capability_flags.unpack(p, end), end), end);
    }

    fixed_integer4 capability_flags; 
    fixed_integer4 max_packet_size; 
    fixed_integer1 character_set; 
};

struct mysql_protocol_ok_packet: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { return b.pack(p, end); }
    char *unpack(char *p, char *end) { return b.unpack(p, end); }
    fixed_integer1 b;
};

struct mysql_protocol_err_packet: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { return b.pack(p, end); }
    char *unpack(char *p, char *end) { return b.unpack(p, end); }
    fixed_integer1 b;
};

struct mysql_protocol_eof_packet: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { return b.pack(p, end); }
    char *unpack(char *p, char *end) { return b.unpack(p, end); }
    fixed_integer1 b;
};


/* return true if connection is still need to keep alive,
 * otherwise stop it */
static void
client_connector_processor(connector_t *connector, packet_t *packet) {
    connection_t *cconn = &connector->ccon;
    connection_t *sconn = &connector->scon;
    switch (cconn->state) {
    case CLIENT_NONE: {
        break;
    }
    case AWAIT_INITIAL_HANDSHAKE_FROM_SERVER: {
        /* determine ssl_connection_request or handshake_response_packet */
        bool is_ssl_handshake_request = packet->get_size() == 4 + 4 + 1 + 23;
        if (is_ssl_handshake_request) {
            mysql_protocol_ssl_request_packet ssl_request;
            ssl_request.unpack(packet->get_pointer(), 
                    packet->get_pointer() + packet->get_size());
            cconn->protocol41 = ssl_request.capability_flags.b & CLIENT_PROTOCOL_41;
            cconn->compress = ssl_request.capability_flags.b & CLIENT_COMPRESS;
            cconn->ssl = ssl_request.capability_flags.b & CLIENT_SSL;

            /* flush immediately, cause switching to ssl */
            cconn->handle(packet);
            drain_output_stream(sconn);

#ifdef HAVE_OPENSSL
            /* switch to ssl */
            vio_reset(sconn->vio, VIO_TYPE_SSL, sconn->fd, 1);
            vio_reset(cconn->vio, VIO_TYPE_SSL, cconn->fd, 1);

            char *cipher = NULL;
            cconn->ssl_connector = new_VioSSLAcceptorFd(server_key, 
                    server_cert, NULL, NULL, NULL);
            sconn->ssl_connector = new_VioSSLConnectorFd(NULL, 
                    NULL, NULL, NULL, NULL);

            if (sslaccept(cconn->ssl_connector, cconn->vio, 60L)) {
#ifdef VERBOSE
                fprintf(stderr, "ssl accept failed\n");
#endif
            }
            
            if (sslconnect(sconn->ssl_connector, sconn->vio, 60L)) { 
#ifdef VERBOSE
                fprintf(stderr, "ssl connect failed\n");
#endif
            }

            vio_blocking(cconn->vio, 0);
            vio_blocking(sconn->vio, 0);
#endif

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
            response.unpack(packet->get_pointer(), 
                    packet->get_pointer() + packet->get_size());
            cconn->protocol41 = response.capability_flags.b & CLIENT_PROTOCOL_41;
            cconn->compress = response.capability_flags.b & CLIENT_COMPRESS;
            cconn->ssl = response.capability_flags.b & CLIENT_SSL;
        }
        cconn->state = STATUS_PACKET_FROM_SERVER;
        sconn->state = AWAIT_STATUS_PACKET_FROM_SERVER;
        cconn->handle(packet);
        break;
    }
    case STATUS_PACKET_FROM_SERVER: {
        break;
    }
    case CLIENT_COMMAND_PHASE: {
        sconn->output_stream->reset_seqid();
        cconn->handle(packet);
        break;
    }
    };
}

static void
server_connector_processor(connector_t *connector, packet_t *packet) {
    connection_t *cconn = &connector->ccon;
    connection_t *sconn = &connector->scon;
    switch (sconn->state) {
    case SERVER_NONE: {
        sconn->state = INITIAL_HANDSHAKE_FROM_SERVER;
        cconn->state = AWAIT_INITIAL_HANDSHAKE_FROM_SERVER;

        mysql_protocol_handshake_packet initial_handshake;
        initial_handshake.unpack(packet->get_pointer(), 
            packet->get_pointer() + packet->get_size());

#ifdef DEBUG
        fprintf(stderr, "%x, %x, server capabilities: %x\n", 
            initial_handshake.proto_version.b,
            initial_handshake.filler.b,
            initial_handshake.capability_flags.b);
#endif

        sconn->protocol41 = initial_handshake.capability_flags.b 
            & CLIENT_PROTOCOL_41;
        sconn->ssl = initial_handshake.capability_flags.b & CLIENT_SSL;
        sconn->compress = initial_handshake.capability_flags.b & CLIENT_COMPRESS;
        sconn->handle(packet);
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

            /* flush immediately, cause turning on compression */
            sconn->handle(packet);
            drain_output_stream(cconn);
        }
        break;
    }
    case SERVER_COMMAND_PHASE: {
        sconn->handle(packet);
        break;
    }
    };
}

static void 
maybe_switch_connector_to_compression(connector_t *conn) {
    connection_t *cconn = &conn->ccon;
    connection_t *sconn = &conn->scon;
    if (!cconn->compression_was_switched
        && sconn->state == SERVER_COMMAND_PHASE
        && cconn->state == CLIENT_COMMAND_PHASE
        && sconn->compress && cconn->compress)
    {
#ifdef DEBUG
        fprintf(stderr, "Switching to compression\n");
#endif
        mysql_packet_seqid *seqid = new mysql_packet_seqid();
        mysql_packet_seqid *compressed_seqid = new mysql_packet_seqid();
        sconn->input_stream = new mysql_packet_input_stream_compressed();
        sconn->output_stream = new mysql_packet_output_stream_compressed(
            seqid, compressed_seqid);
        cconn->input_stream = new mysql_packet_input_stream_compressed();
        cconn->output_stream = new mysql_packet_output_stream_compressed(
                seqid, compressed_seqid);

        sconn->packet_handlers.clear();
        cconn->packet_handlers.clear();

        sconn->packet_handlers.push_back(new mysql_write_handler(
            cconn->output_stream));

        cconn->packet_handlers.push_back(new mysql_write_handler(
            sconn->output_stream));

        cconn->compression_was_switched = sconn->compression_was_switched = true;
    }
}

static void
server_connector_handler(int fd, short event, void *arg) {
    bool finalize = false;
    size_t was_read; uchar buf[1024];
    connector_t *connector = (connector_t *)arg;

    do {
        was_read = vio_read(connector->scon.vio, buf, sizeof(buf));
        if (was_read == (size_t)-1 || !was_read) { 
            if (was_read == (size_t)-1 && (errno == EAGAIN || errno == EINTR)) {
                continue;
            } else {
                finalize = true;
                break;
            }
        }

        connector->scon.input_stream->feed_data(buf, was_read);
    } while (was_read == sizeof(buf));

#ifdef DEBUG
    fprintf(stderr, "got %d bytes from server\n", was_read);
    fprintf(stderr, "client state: %d\n", connector->ccon.state);
    fprintf(stderr, "server state: %d\n", connector->scon.state);
    fprintf(stderr, "server queue size: %d\n", 
            connector->scon.input_stream->packet_count());
#endif

    while (connector->scon.input_stream->packet_count()) {
        packet_t *packet = connector->scon.input_stream->packet_top();
        server_connector_processor(connector, packet);
        connector->scon.input_stream->packet_pop();
    }
    event_add(&connector->scon.event, NULL);

#ifdef DEBUG
    fprintf(stderr, "toclient output queue size: %d\n", 
            connector->ccon.output_stream->packet_count());
#endif

    drain_output_stream(&connector->ccon);

#ifdef DEBUG
    fprintf(stderr, "toclient output queue size: %d\n", 
            connector->ccon.output_stream->packet_count());
    fprintf(stderr, "server queue size: %d\n", 
            connector->scon.input_stream->packet_count());
#endif

    maybe_switch_connector_to_compression(connector);
    if (finalize) { finalize_connector(connector); }
}

static void
client_connector_handler(int fd, short event, void *arg) {
    bool finalize = false;
    size_t was_read; uchar buf[1024];
    connector_t *connector = (connector_t *)arg;

    do {
        was_read = vio_read(connector->ccon.vio, buf, sizeof(buf));
        if (was_read == (size_t)-1 || !was_read) { 
            if (was_read == (size_t)-1 && (errno == EAGAIN || errno == EINTR)) {
                continue;
            } else {
                finalize = true;
                break;
            }
        }

        connector->ccon.input_stream->feed_data(buf, was_read);
    } while (was_read == sizeof(buf));

#ifdef DEBUG
    fprintf(stderr, "got %d bytes from client\n", was_read);
    fprintf(stderr, "client state: %d\n", connector->ccon.state);
    fprintf(stderr, "server state: %d\n", connector->scon.state);
    fprintf(stderr, "client queue size: %d\n", 
            connector->ccon.input_stream->packet_count());
#endif

    while (connector->ccon.input_stream->packet_count()) {
        client_connector_processor(connector,
                connector->ccon.input_stream->packet_top());
        connector->ccon.input_stream->packet_pop();
    }
    event_add(&connector->ccon.event, NULL);

#ifdef DEBUG
    fprintf(stderr, "toserver output queue size: %d\n", 
            connector->scon.output_stream->packet_count());
#endif

    drain_output_stream(&connector->scon);

#ifdef DEBUG
    fprintf(stderr, "toserver output queue size: %d\n", 
            connector->scon.output_stream->packet_count());
    fprintf(stderr, "client queue size: %d\n", 
            connector->ccon.input_stream->packet_count());
#endif
    if (finalize) { finalize_connector(connector); }
}

static void 
acceptor_handler(int fd, short event, void *arg) {
    acceptor_t *acceptor = (acceptor_t *)arg;
    int peer_fd = accept(acceptor->fd, NULL, NULL);
    if (peer_fd < 0) {
        return;
    }

    connector_t *connector = new connector_t();
    connector->ccon.fd = peer_fd;

    int server_fd;
    if ((server_fd = connect_to_peer(k_mysql_server, k_mysql_server_port)) >= 0) {
        connector->scon.fd = server_fd; 

        /* flags=1 localhost here */
        connector->ccon.vio = vio_new(connector->ccon.fd, VIO_TYPE_TCPIP, 1);
        connector->scon.vio = vio_new(connector->scon.fd, VIO_TYPE_TCPIP, 1);

        /* setting as nonblocking and keepalive */
        vio_blocking(connector->ccon.vio, 0);
        vio_blocking(connector->scon.vio, 0);
        vio_keepalive(connector->ccon.vio, 1);
        vio_keepalive(connector->scon.vio, 1);

        mysql_packet_seqid *seqid = new mysql_packet_seqid();

        connector->scon.input_stream = new mysql_packet_input_stream();
        connector->scon.output_stream = new mysql_packet_output_stream(seqid);

        connector->ccon.input_stream = new mysql_packet_input_stream();
        connector->ccon.output_stream = new mysql_packet_output_stream(seqid);

        connector->ccon.state = CLIENT_NONE;
        connector->scon.state = SERVER_NONE;

        connector->ccon.packet_handlers.push_back(new mysql_statistics_handler());
        connector->ccon.packet_handlers.push_back(new mysql_write_handler(
            connector->scon.output_stream));

        connector->scon.packet_handlers.push_back(new mysql_write_handler(
            connector->ccon.output_stream));

#ifdef DEBUG
        connector->ccon.dumper = new hexdumper<>("toclient.log");
        connector->scon.dumper = new hexdumper<>("toserver.log");

        connector->ccon.packet_dumper = new hexdumper<>("mytoclient.log");
        connector->scon.packet_dumper = new hexdumper<>("mytoserver.log");

        connector->ccon.packet_offset_dumper = new hexdumper<8>("offset_dumper_to_client.log");
        connector->scon.packet_offset_dumper = new hexdumper<8>("offset_dumper_to_server.log");
#endif
        event_set(&connector->ccon.event, connector->ccon.fd, 
            EV_READ, client_connector_handler, (void *)connector);
        event_set(&connector->scon.event, connector->scon.fd, 
            EV_READ, server_connector_handler, (void *)connector);
        regenerate_connector_event(connector);
    } else {
        close(peer_fd);
        delete connector;
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
    event_init();

    acceptor_t acceptor;
    if (initialize_acceptor(&acceptor, k_proxy_listen_port) < 0) {
        return 1;
    }

    event_dispatch();
    return 0;
}

