
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "mysqlcommon.h"

#include <string>
#include <deque>
#include <vector>
#include <fstream>

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

const int MAX_PACKET_LENGTH = 256L*256L*256L-1;

namespace {
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

    void append(const boost::shared_ptr<fragment_t> &fragment) {
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
    packet_output_stream(boost::shared_ptr<mysql_packet_seqid> aseqid)
        : seqid(aseqid)
    {}

    packet_output_stream(boost::shared_ptr<mysql_packet_seqid> aseqid, 
            boost::shared_ptr<mysql_packet_seqid> acompressedseqid) 
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
        if (compressed_seqid.get()) {
            compressed_seqid->reset(); 
        }
    }

protected:
    boost::shared_ptr<mysql_packet_seqid> seqid;
    boost::shared_ptr<mysql_packet_seqid> compressed_seqid;

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
    mysql_write_handler(const boost::shared_ptr<packet_output_stream> &s)
        : output_stream(s)
    {}

    void handle(packet_t *packet) {
        output_stream->feed_packet(packet);
    }

private:
    boost::shared_ptr<packet_output_stream> output_stream;
};

class mysql_packet_output_stream: public packet_output_stream {
public:
    mysql_packet_output_stream(const boost::shared_ptr<mysql_packet_seqid> &aseqid) 
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
    mysql_packet_output_stream_compressed(
            const boost::shared_ptr<mysql_packet_seqid> &aseqid, 
            const boost::shared_ptr<mysql_packet_seqid> &acompressedseqid) 
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
    virtual boost::shared_ptr<packet_t> packet_top() const = 0;
    virtual void packet_pop() = 0;
    virtual int  packet_count() const = 0;
};

class mysql_packet_input_stream: public packet_input_stream  {
public:
    mysql_packet_input_stream() 
    {}

    ~mysql_packet_input_stream() 
    {}

    void feed_data(uchar *data, size_t len) {
        if (pending_fragments.empty()) {
            pending_fragments.push_back(
                boost::shared_ptr<fragment_t>(new fragment_t()));
        }

        while (len) {
            boost::shared_ptr<fragment_t> fragment = pending_fragments.back();

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
                pending_fragments.push_back(
                    boost::shared_ptr<fragment_t>(new fragment_t()));
            }
        }
    }

    boost::shared_ptr<packet_t> packet_top() const {
        assert(packet_count() > 0);
        return output_packets.front();
    }

    void packet_pop() {
        assert(packet_count() > 0);
        output_packets.pop_front();
    }

    int packet_count() const {
        return output_packets.size();
    }

private:
    void add_fragment_to_output(const boost::shared_ptr<fragment_t> &fragment) {
        assert(fragment->is_complete());
        if (fragment->is_last()) {
            flush_pending_fragments_to_output();
        }
    }

    void flush_pending_fragments_to_output() {
        boost::shared_ptr<packet_t> packet(new packet_t());
        for (int i = 0; i < pending_fragments.size(); ++i) {
            packet->append(pending_fragments[i]);
        }
        output_packets.push_back(packet);
        pending_fragments.clear();
    }

private:
    std::deque<boost::shared_ptr<packet_t> > output_packets;
    std::deque<boost::shared_ptr<fragment_t> > pending_fragments;
};


class mysql_packet_input_stream_compressed: public packet_input_stream {
public:
    mysql_packet_input_stream_compressed() 
    {}

    ~mysql_packet_input_stream_compressed() 
    {}

    void feed_data(uchar *data, size_t len) {
        if (compressed_pending_fragments.empty()) {
            compressed_pending_fragments.push_back(
                boost::shared_ptr<compressed_fragment_t>(new compressed_fragment_t()));
        }

        while (len) {
            boost::shared_ptr<compressed_fragment_t> fragment 
                = compressed_pending_fragments.back();

            uchar *newdata = (uchar*)fragment->append((char*)data, len);
            len -= newdata - data;
            data = newdata;

            if (fragment->is_complete()) {
                add_fragment_to_pending(fragment);
                compressed_pending_fragments.push_back(
                    boost::shared_ptr<compressed_fragment_t>(
                        new compressed_fragment_t()));
            }
        }
    }

    boost::shared_ptr<packet_t> packet_top() const {
        assert(packet_count() > 0);
        return output_packets.front();
    }

    void packet_pop() {
        assert(packet_count() > 0);
        output_packets.pop_front();
    }

    int packet_count() const {
        return output_packets.size();
    }

private:
    void add_fragment_to_pending(boost::shared_ptr<compressed_fragment_t> fragment) {
        assert(fragment->is_complete());
        if (fragment->is_last()) {
            flush_compressed_pending_fragments_to_pending_fragments();
        }
    }

    void flush_compressed_pending_fragments_to_pending_fragments() {
        for (int i = 0; i < compressed_pending_fragments.size(); ++i) {
            boost::shared_ptr<compressed_fragment_t> fragment 
                = compressed_pending_fragments[i];

            char *pointer = fragment->get_pointer();
            size_t uncompressed_size = fragment->get_uncompressed_size();
            while (uncompressed_size) {
                if (pending_fragments.empty()) {
                    pending_fragments.push_back(
                        boost::shared_ptr<fragment_t>(new fragment_t()));
                }

                boost::shared_ptr<fragment_t> result = pending_fragments.back();
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

    void add_fragment_to_output(const boost::shared_ptr<fragment_t> &fragment) {
        assert(fragment->is_complete());
        if (fragment->is_last()) {
            flush_pending_fragments_to_output();
        } else {
            pending_fragments.push_back(boost::shared_ptr<fragment_t>(new fragment_t()));
        }
    }

    void flush_pending_fragments_to_output() {
        boost::shared_ptr<packet_t> packet(new packet_t());
        for (int i = 0; i < pending_fragments.size(); ++i) {
            boost::shared_ptr<fragment_t> fragment = pending_fragments[i];
            packet->append(fragment);
        }
        output_packets.push_back(packet);
        pending_fragments.clear();
    }

private:
    std::deque<boost::shared_ptr<packet_t> > output_packets;
    std::deque<boost::shared_ptr<fragment_t> > pending_fragments;
    std::deque<boost::shared_ptr<compressed_fragment_t> > compressed_pending_fragments;
};


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
    nullterminated_string(): b(NULL) 
    {}

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
    eof_string() : b(NULL), size(0)
    {}

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

struct connection_settings {
    connection_settings()
      : mysqlserver_port(0)
    {}

    std::string mysqlserver_address;
    int mysqlserver_port;

    std::string client_key;
    std::string client_cert;
    std::string client_ca_file;

    std::string server_key;
    std::string server_cert;
    std::string server_ca_file;

    std::string  dh_file;
};

class mysql_connection {
public:
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
    
    enum { BufferSize = 4096 };

private:
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> 
        ssl_stream_type;

public:
    mysql_connection(connection_settings settings, 
            boost::asio::io_service &io_service)
        : 
          io_service_(io_service),
          settings_(settings),
          client_socket_(io_service),
          server_socket_(io_service),
          client_context_(io_service, boost::asio::ssl::context::tlsv1),
          server_context_(io_service, boost::asio::ssl::context::tlsv1)
    {}

    boost::asio::ip::tcp::socket &client_socket() {
        return client_socket_;
    }

    boost::asio::ip::tcp::socket &server_socket() {
        return server_socket_;
    }

    void init() {
        namespace ip = boost::asio::ip;
        ip::tcp::endpoint endpoint(ip::address::from_string(
                    settings_.mysqlserver_address), settings_.mysqlserver_port);
        server_socket_.connect(endpoint);

        client_capabilities_ = 0;
        server_capabilities_ = 0;

        client_state_ = CLIENT_NONE;
        server_state_ = SERVER_NONE;

        boost::shared_ptr<mysql_packet_seqid> seqid(new mysql_packet_seqid());
        client_input_stream_.reset(new mysql_packet_input_stream());
        client_output_stream_.reset(new mysql_packet_output_stream(seqid));

        server_input_stream_.reset(new mysql_packet_input_stream());
        server_output_stream_.reset(new mysql_packet_output_stream(seqid));

        client_packet_handlers_.clear();
        client_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_write_handler(server_output_stream_)));

        server_packet_handlers_.clear();
        server_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_write_handler(client_output_stream_)));

        compression_decision_determined_ = false;
        ssl_desicion_determined_ = false;
        in_ssl_ = false;

        init_client_read();
        init_server_read();
    }

private:

    void maybe_turnon_compression() {
        if (compression_decision_determined_)
            return;

        if (server_state_ == SERVER_COMMAND_PHASE
            && client_state_ == CLIENT_COMMAND_PHASE)
        {
            if (server_capabilities_ & CLIENT_COMPRESS
                && client_capabilities_ & CLIENT_COMPRESS)
            {
                turnon_compression();
            }
            compression_decision_determined_ = true;
        }
    }

    void turnon_compression() {
#ifdef DEBUG
        fprintf(stderr, "Switching to compression\n");
#endif
        boost::shared_ptr<mysql_packet_seqid> seqid(new mysql_packet_seqid());
        boost::shared_ptr<mysql_packet_seqid> compressed_seqid(new mysql_packet_seqid());

        client_input_stream_.reset(new mysql_packet_input_stream_compressed());
        client_output_stream_.reset(new mysql_packet_output_stream_compressed(seqid,
                    compressed_seqid));

        server_input_stream_.reset(new mysql_packet_input_stream_compressed());
        server_output_stream_.reset(new mysql_packet_output_stream_compressed(seqid,
                    compressed_seqid));

        client_packet_handlers_.clear();
        client_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_write_handler(server_output_stream_)));

        server_packet_handlers_.clear();
        server_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_write_handler(client_output_stream_)));
    }

    void maybe_turnon_ssl() {
        if (ssl_desicion_determined_)
            return;

        if (client_state_ == SSL_CONNECTION_REQUEST) {
            turnon_ssl();
        } else if (client_state_ > SSL_CONNECTION_REQUEST) {
            /* no ssl */
            ssl_desicion_determined_ = true;
        }
    }

    void turnon_ssl() {
        client_context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
        client_context_.use_certificate_chain_file(settings_.server_cert);
        client_context_.use_private_key_file(
            settings_.server_key, boost::asio::ssl::context::pem);
        client_context_.use_tmp_dh_file(settings_.dh_file);

        client_socket_ssl_stream_.reset(
            new ssl_stream_type(io_service_, client_context_));
        server_socket_ssl_stream_.reset(
            new ssl_stream_type(io_service_, server_context_));

        client_socket_ssl_stream_->handshake(boost::asio::ssl::stream_base::server);
        server_socket_ssl_stream_->handshake(boost::asio::ssl::stream_base::client);

        in_ssl_ = true;
    }

    void init_client_read() {
        if (in_ssl_) {
            client_socket_ssl_stream_->async_read_some(
                boost::asio::buffer(client_buffer_),
                boost::bind(&mysql_connection::on_client_read, this,
                    boost::asio::placeholders::error, 
                    boost::asio::placeholders::bytes_transferred));
        } else {
            boost::asio::async_read(client_socket_,
                boost::asio::buffer(client_buffer_),
                boost::asio::transfer_at_least(1),
                boost::bind(&mysql_connection::on_client_read, this,
                    boost::asio::placeholders::error, 
                    boost::asio::placeholders::bytes_transferred));
        }
    }

    void on_client_read(const boost::system::error_code& error,
            std::size_t bytes_transferred) 
    {
        if (!bytes_transferred
                && !(error.value() == boost::system::posix::operation_would_block 
           || error.value() == boost::system::posix::resource_unavailable_try_again
           || error.value() == boost::system::posix::interrupted))
        {
            disconnect();
            return;
        }

        client_input_stream_->feed_data((uchar*)client_buffer_.begin(),
                bytes_transferred);
        while (client_input_stream_->packet_count()) {
            boost::shared_ptr<packet_t> packet = client_input_stream_->packet_top();
            on_client_packet(packet.get());
            client_input_stream_->packet_pop();
        }

        flush_toserver_queue();

        maybe_turnon_ssl();

        init_client_read();
    }

    void on_client_packet(packet_t *packet) {
        switch (client_state_) {
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
                client_capabilities_ = ssl_request.capability_flags.b;
    
                /* flush immediately, cause switching to ssl */
                send_to_server(packet);
    
                client_state_ = SSL_CONNECTION_REQUEST;
                return;
            }
            /* else handshake_response is there, just fall through */
        }
        case SSL_CONNECTION_REQUEST:
        case HANDSHAKE_RESPONSE_PACKET: {
            /* do not initialize twice */
            if (client_state_ != SSL_CONNECTION_REQUEST) {
                mysql_protocol_handshake_response_packet response;
                response.unpack(packet->get_pointer(), 
                        packet->get_pointer() + packet->get_size());
                client_capabilities_ = response.capability_flags.b;
            }
            client_state_ = STATUS_PACKET_FROM_SERVER;
            server_state_ = AWAIT_STATUS_PACKET_FROM_SERVER;
            send_to_server(packet);
            break;
        }
        case STATUS_PACKET_FROM_SERVER: {
            break;
        }
        case CLIENT_COMMAND_PHASE: {
            server_output_stream_->reset_seqid();
            send_to_server(packet);
            break;
        }};
    }

    void send_to_server(packet_t *packet) {
        for (int i = 0; i < client_packet_handlers_.size(); ++i) {
            client_packet_handlers_[i]->handle(packet);
        }
    }

    void send_to_client(packet_t *packet) {
        for (int i = 0; i < server_packet_handlers_.size(); ++i) {
            server_packet_handlers_[i]->handle(packet);
        }
    }

    int write_nbytes_to_connection(boost::asio::ip::tcp::socket &sk,
            char *buf, size_t size) 
    {
        size_t left = size;
        while (left) {
            size_t written_bytes = sk.write_some(boost::asio::buffer(buf, left));
            if (!written_bytes || written_bytes == static_cast<size_t>(-1)) {
                if (written_bytes && (errno == EINTR || errno == EAGAIN))
                    continue;
                return 1;
            }
            buf += written_bytes;
            left -= written_bytes;
        }
        return 0;
    }

    int write_nbytes_to_connection_ssl(ssl_stream_type &sk,
            char *buf, size_t size) 
    {
        size_t left = size;
        while (left) {
            size_t written_bytes = sk.write_some(boost::asio::buffer(buf, left));
            if (!written_bytes || written_bytes == static_cast<size_t>(-1)) {
                if (written_bytes && (errno == EINTR || errno == EAGAIN))
                    continue;
                return 1;
            }
            buf += written_bytes;
            left -= written_bytes;
        }
        return 0;
    }

    void flush_toserver_queue() {
        packet_output_stream *output_stream = server_output_stream_.get();
        if (output_stream->packet_count()) {
            char *data; size_t write_len; int res;
            output_stream->get_packets(&data, &write_len);
            if (in_ssl_) {
                res = write_nbytes_to_connection_ssl(
                    *(server_socket_ssl_stream_.get()), data, write_len);
            } else {
                res = write_nbytes_to_connection(
                    server_socket_, data, write_len);
            }
        }
    }

    void flush_toclient_queue() {
        packet_output_stream *output_stream = client_output_stream_.get();
        if (output_stream->packet_count()) {
            char *data; size_t write_len; int res;
            output_stream->get_packets(&data, &write_len);
            if (in_ssl_) {
                res = write_nbytes_to_connection_ssl(
                    *(client_socket_ssl_stream_.get()), data, write_len);
            } else {
                res = write_nbytes_to_connection(
                    client_socket_, data, write_len);
            }
        }
    }

    void init_server_read() {
        if (in_ssl_) {
            server_socket_ssl_stream_->async_read_some(
                boost::asio::buffer(server_buffer_),
                boost::bind(&mysql_connection::on_server_read, this,
                boost::asio::placeholders::error, 
                boost::asio::placeholders::bytes_transferred));
        } else {
            boost::asio::async_read(server_socket_,
                boost::asio::buffer(server_buffer_),
                boost::asio::transfer_at_least(1),
                boost::bind(&mysql_connection::on_server_read, this,
                    boost::asio::placeholders::error, 
                    boost::asio::placeholders::bytes_transferred));
        }
    }

    void on_server_read(const boost::system::error_code& error,
            std::size_t bytes_transferred) 
    {
        if (!bytes_transferred
                && !(error.value() == boost::system::posix::operation_would_block 
           || error.value() == boost::system::posix::resource_unavailable_try_again
           || error.value() == boost::system::posix::interrupted))
        {
            disconnect();
            return;
        }

        server_input_stream_->feed_data((uchar*)server_buffer_.begin(), 
                bytes_transferred);
        while (server_input_stream_->packet_count()) {
            boost::shared_ptr<packet_t> packet = server_input_stream_->packet_top();
            on_server_packet(packet.get());
            server_input_stream_->packet_pop();
        }

        flush_toclient_queue();

        maybe_turnon_compression();

        init_server_read();
    }

    void on_server_packet(packet_t *packet) {
        switch (server_state_) {
        case SERVER_NONE: {
            server_state_ = INITIAL_HANDSHAKE_FROM_SERVER;
            client_state_ = AWAIT_INITIAL_HANDSHAKE_FROM_SERVER;
    
            mysql_protocol_handshake_packet initial_handshake;
            initial_handshake.unpack(packet->get_pointer(), 
                packet->get_pointer() + packet->get_size());
    
    #ifdef DEBUG
            fprintf(stderr, "%x, %x, server capabilities: %x\n", 
                initial_handshake.proto_version.b,
                initial_handshake.filler.b,
                initial_handshake.capability_flags.b);
    #endif
    
            server_capabilities_ = initial_handshake.capability_flags.b;
            send_to_client(packet);
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
                server_state_ = SERVER_COMMAND_PHASE;
                client_state_ = CLIENT_COMMAND_PHASE;
                send_to_client(packet);
            }
            break;
        }
        case SERVER_COMMAND_PHASE: {
            send_to_client(packet);
            break;
        }};
    }

    void disconnect() {
        try {
            client_socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            client_socket_.close();
            server_socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            server_socket_.close();
        } catch (...) {
        }
    }

private:
    boost::asio::io_service &io_service_;

    connection_settings settings_;
    boost::asio::ip::tcp::socket client_socket_;
    boost::asio::ip::tcp::socket server_socket_;

    boost::shared_ptr<ssl_stream_type> client_socket_ssl_stream_;
    boost::shared_ptr<ssl_stream_type> server_socket_ssl_stream_;

    uint client_capabilities_;
    ClientFlags client_state_;
    std::vector<boost::shared_ptr<mysql_packet_handler> > client_packet_handlers_;
    boost::shared_ptr<packet_input_stream> client_input_stream_;
    boost::shared_ptr<packet_output_stream> client_output_stream_;
    boost::array<char, BufferSize> client_buffer_;

    uint server_capabilities_;
    ServerFlags server_state_;
    std::vector<boost::shared_ptr<mysql_packet_handler> > server_packet_handlers_;
    boost::shared_ptr<packet_input_stream> server_input_stream_;
    boost::shared_ptr<packet_output_stream> server_output_stream_;
    boost::array<char, BufferSize> server_buffer_;

    boost::asio::ssl::context client_context_;
    boost::asio::ssl::context server_context_;

    bool compression_decision_determined_;
    bool ssl_desicion_determined_;
    bool in_ssl_;
};

class mysqlproxy {
public:
    mysqlproxy(
        const std::string &bind_host,
        int bind_port,
        const connection_settings &settings)
    : bind_host_(bind_host),
      bind_port_(bind_port),
      settings_(settings),
      io_service_(),
      acceptor_(io_service_)
    {
        namespace ip = boost::asio::ip;
        ip::tcp::endpoint endpoint(ip::address::from_string(bind_host_), bind_port_);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        start_accept();
    }

    void on_accept(const boost::system::error_code& error) {
        if (error) {
            return;
        }

        new_connection_->init();

        start_accept();
    }

    void run() { io_service_.run(); }

private:
    void start_accept() {
        new_connection_ = new mysql_connection(settings_, io_service_); //TODO
        acceptor_.async_accept(new_connection_->client_socket(),
            boost::bind(&mysqlproxy::on_accept, this,
                boost::asio::placeholders::error));
    }

private:
    std::string bind_host_;
    int bind_port_;

    connection_settings settings_;

    boost::asio::io_service io_service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    mysql_connection *new_connection_;
};

/* constants */
const char *k_mysql_server = "127.0.0.1";
const char *k_proxy_listen_address = "127.0.0.1";
const int k_proxy_listen_port = 19999;
const int k_mysql_server_port = 40000;

const char *k_client_key = "certsdir/client-key.pem";
const char *k_client_cert = "certsdir/client-cert.pem";
const char *k_client_ca_file = "certsdir/ca-cert.pem";

const char *k_server_key = "certsdir/server-key.pem";
const char *k_server_cert = "certsdir/server-cert.pem";
const char *k_server_ca_file = "certsdir/ca-cert.pem";

const char *k_dh_file = "certsdir/dh512.pem";

int main(int argc, char **argv) {

    connection_settings settings;
    settings.mysqlserver_address = k_mysql_server;
    settings.mysqlserver_port = k_mysql_server_port;
    settings.client_key = k_client_key;
    settings.client_cert = k_client_cert;
    settings.client_ca_file = k_client_ca_file;
    settings.server_key = k_server_key;
    settings.server_cert = k_server_cert;
    settings.server_ca_file = k_server_ca_file;
    settings.dh_file = k_dh_file;

    mysqlproxy proxy(k_proxy_listen_address, k_proxy_listen_port, settings);
    proxy.run();

    return 0;
}

