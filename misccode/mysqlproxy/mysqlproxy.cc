
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "mysqlcommon.h"

#include <string>
#include <deque>
#include <vector>
#include <set>
#include <fstream>

#include <MyIsamLib.h>
#include <Query.h>

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem/operations.hpp>

extern "C" {
#include <EXTERN.h>
#include <perl.h>
}

const int MAX_PACKET_LENGTH = 256L*256L*256L-1;

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

    void set_content(const std::string &content) {
        packet_data = content;
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
};

class mysql_packet_handler {
public:
    ~mysql_packet_handler() {}
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
            output.append((char*)packet_header, sizeof(packet_header));
            output.append((char*)data, MAX_PACKET_LENGTH);

            data += MAX_PACKET_LENGTH;
            len -= MAX_PACKET_LENGTH;

            output_packet_fragments_count++;
        }

        int3store(packet_header, len);
        packet_header[3] = (uchar)(*seqid)++;

        output.append((char*)packet_header, sizeof(packet_header));
        output.append((char*)data, len);

        output_packet_fragments_count++;
        output_packet_count++;
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

template <typename T>
static char *unpack_from_packet(T &dstpacket, packet_t *packet) {
    return dstpacket.unpack(packet->get_pointer(), 
            packet->get_pointer() + packet->get_size());
}

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
struct length_encoded_integer: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) {
        if (b < 251) {
            *p++ = static_cast<char>(b);
        } else if (b < 1<<16) {
            *p++ = 0xfc;
            int2store(p, b);
            p += 2;
        } else if (b < 1<<24) {
            *p++ = 0xfd;
            int3store(p, b);
            p += 3;
        } else {
            *p++ = 0xfe;
            int8store(p, b);
            p += 8;
        }
        return p;
    }

    char *unpack(char *p, char *end) {
        unsigned char len = static_cast<unsigned char>(*p);
        if (len < 0xfb) { b = len; p++; }
        else {
            p++;
            if (len == 0xfc) { b = uint2korr(p); p += 2; }
            else if (len == 0xfd) { b = uint3korr(p); p += 3; }
            else if (len == 0xfe) { b = uint8korr(p); p += 8; }
        }
        return p;
    }
    
    ulonglong b;
};

template <int n>
struct fixedlen_string: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { memcpy(p, b, n); return p + n; }
    char *unpack(char *p, char *end) { memcpy(b, p, n); return p + n; }
    uchar b[n];
};

struct length_encoded_string: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) { 
        p = length.pack(p, end);
        memcpy(p, b.c_str(), length.b);
        return p + length.b;
    }
    char *unpack(char *p, char *end) { 
        p = length.unpack(p, end);
        b = std::string(p, p + length.b);
        return p + length.b;
    }
    length_encoded_integer length;
    std::string b;
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

struct mysql_procotol_com_query_packet: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) {
        return query_string.pack(com_id.pack(p, end), end);
    }
    char *unpack(char *p, char *end) {
        return query_string.unpack(com_id.unpack(p, end), end);
    }

    fixed_integer1 com_id;
    eof_string query_string;
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
    char *pack(char *p, char *end) { 
        return status_flags.pack(
            warning_count.pack(
            b.pack(p, end), end), end);

    }
    char *unpack(char *p, char *end) { 
        return status_flags.unpack(
            warning_count.unpack(
            b.unpack(p, end), end), end);
    }

    fixed_integer1 b;
    fixed_integer2 warning_count;
    fixed_integer2 status_flags;
};

struct mysql_protocol_column_definition: public mysql_protocol_itemtype {
    char *pack(char *p, char *end) {
        return org_name.pack(name.pack(org_table.pack(
            table.pack(schema.pack(catalog.pack(p, 
            end), end), end), end), end), end);
    }
    char *unpack(char *p, char *end) {
        return org_name.unpack(name.unpack(org_table.unpack(
            table.unpack(schema.unpack(catalog.unpack(p, 
            end), end), end), end), end), end);
    }

    length_encoded_string catalog;
    length_encoded_string schema;
    length_encoded_string table;
    length_encoded_string org_table;
    length_encoded_string name;
    length_encoded_string org_name;
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

struct connection_statistics {
    connection_statistics() { memset(m, 0, COM_END * sizeof(int)); }

    void increment(enum_server_command type) {
        m[type]++;
    }

    int m[COM_END];
};

class mysql_connection;
class mysql_statistics_handler: public mysql_packet_handler {
public:
    mysql_statistics_handler(mysql_connection &conn)
        : conn_(conn)
    {}

    void handle(packet_t *packet);

private:
    mysql_connection &conn_;
};

class mysql_client_query_logger: public mysql_packet_handler {
public:
    mysql_client_query_logger(mysql_connection &conn, 
            const std::string &logpath)
        : conn_(conn), fs_(logpath.c_str(), std::ios_base::out)
    {}

    void handle(packet_t *packet);

private:
    mysql_connection &conn_;
    std::fstream fs_;
};

class mysql_password_interceptor {
public:
    enum ClientFlags {
        CLIENT_NONE,
        CLIENT_QUERY_SENT
    };

    enum ServerFlags {
        SERVER_NONE,
        READING_COLUMNS,
        READING_ROWS,
    };

public:
    mysql_password_interceptor(mysql_connection &conn)
        : conn_(conn)
    { reset_state(); }

    mysql_password_interceptor(mysql_connection &conn, 
            const std::vector<std::string> &columns_to_intercept)
        : conn_(conn), columns_to_intercept_(columns_to_intercept)
    { reset_state(); }

    void handle_from_client(packet_t *packet);
    void handle_from_server(packet_t *packet);
    void reset_state();

private:
    void compute_intercept_indices();
    void perform_intercept();

    mysql_connection &conn_;
    ClientFlags client_state_;
    ServerFlags server_state_;

    ulonglong table_columns_count_;
    ulonglong columns_to_read_;

    std::vector<std::string> columns_to_intercept_;
    std::vector<int> columns_to_intercept_indices_;

    std::vector<std::string> column_definitions_;
    std::vector<std::string> current_row_;
    std::vector<bool> current_row_null_fields_;
};

class mysql_client_password_interceptor: public mysql_packet_handler {
public:
    mysql_client_password_interceptor(
        const boost::shared_ptr<mysql_password_interceptor> &interceptor)
        : interceptor_(interceptor)
    {}

    void handle(packet_t *packet) {
        interceptor_->handle_from_client(packet);
    }

private:
    boost::shared_ptr<mysql_password_interceptor> interceptor_;
};

class mysql_server_password_interceptor: public mysql_packet_handler {
public:
    mysql_server_password_interceptor(
        const boost::shared_ptr<mysql_password_interceptor> &interceptor)
        : interceptor_(interceptor)
    {}

    void handle(packet_t *packet) {
        interceptor_->handle_from_server(packet);
    }

private:
    boost::shared_ptr<mysql_password_interceptor> interceptor_;
};

class perl_interpreter {
public:
    perl_interpreter(const std::string &funclib_filename)
    {
        int argc = 2;
        const char *argv[] = { "stub", funclib_filename.c_str(), NULL, };
        PERL_SYS_INIT3(&argc, reinterpret_cast<char***>(&argv), NULL);
        my_perl= perl_alloc();
        perl_construct(my_perl);
        perl_parse(my_perl, NULL, argc, reinterpret_cast<char**>(
                    const_cast<char**>(argv)), NULL);
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    }

    ~perl_interpreter() {
        perl_destruct(my_perl);
        perl_free(my_perl);
    }

    std::string call(const std::string &funcname, const std::string &arg) 
    {
        int count;
        std::string result; 
        char *result_cstr; STRLEN result_length;

        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        XPUSHs(sv_2mortal(newSVpv(arg.c_str(), arg.length())));
        PUTBACK;

        count = call_pv(const_cast<char*>(funcname.c_str()), G_SCALAR);

        SPAGAIN;

        SV *s = POPs;
        result_cstr = SvPV(s, result_length);
        result = std::string(result_cstr, result_length);

        PUTBACK;
        FREETMPS;
        LEAVE;

        return result;
    }

    std::string call(const std::string &funcname, SV *query_repr) 
    {
        int count;
        std::string result; 
        char *result_cstr; STRLEN result_length;

        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        XPUSHs(sv_2mortal(query_repr));
        PUTBACK;

        count = call_pv(const_cast<char*>(funcname.c_str()), G_SCALAR);

        SPAGAIN;

        SV *s = POPs;
        result_cstr = SvPV(s, result_length);
        result = std::string(result_cstr, result_length);

        PUTBACK;
        FREETMPS;
        LEAVE;

        return result;
    }

    SV *string2sv(const std::string &str) {
        return newSVpvn(str.c_str(), str.size());
    }
    
    SV *transform_query_union_to_perlsv(const myisamlib::QueryUnion &q) {
        AV *av = newAV(); 
        for (int i = 0; i < q.query_union_list_.size(); ++i) {
            av_push(av, transform_query_to_perlhv(q.query_union_list_[0]));
        }
        return newRV(reinterpret_cast<SV*>(av));
    }
    
    SV *transform_query_to_perlhv(const myisamlib::Query &q) {
        HV *hv = newHV(); 
    
        struct {
            const char *key;
            std::vector<std::string> myisamlib::Query::* p; 
        } lists[] = {
            { "items", &myisamlib::Query::item_list_ },
            { "where", &myisamlib::Query::where_list_ },
            { "group", &myisamlib::Query::group_list_ },
            { "having", &myisamlib::Query::having_list_ },
            { "order", &myisamlib::Query::order_list_ },
            { "limit", &myisamlib::Query::limit_list_ } 
        };
    
        for (int i = 0; i < sizeof(lists)/sizeof(lists[0]); ++i) {
            AV *av = newAV();
    
            const std::vector<std::string> &vlist = q.*(lists[i].p);
            for (int j = 0; j < vlist.size(); ++j) {
                av_push(av, string2sv(vlist[j]));
            }
    
            hv_store(hv, lists[i].key, strlen(lists[i].key), 
                    newRV(reinterpret_cast<SV*>(av)), 0);
        }
        return newRV(reinterpret_cast<SV*>(hv));
    }

private:
    PerlInterpreter *my_perl;
};

class mysql_query_perl_processor: public mysql_packet_handler  {
public:
    mysql_query_perl_processor(mysql_connection &conn,
        const std::string &funclib_filename)
        : conn_(conn), p_(funclib_filename)
    {}

    void handle(packet_t *packet);

private:
    mysql_connection &conn_;
    perl_interpreter p_;
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
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> 
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

        std::vector<std::string> fields_to_intercept;
        fields_to_intercept.push_back("user");
        fields_to_intercept.push_back("password");

        boost::shared_ptr<mysql_password_interceptor> interceptor(
                new mysql_password_interceptor(*this, fields_to_intercept));

        client_packet_handlers_.clear();
        client_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_client_password_interceptor(interceptor)));
        client_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_query_perl_processor(*this, 
                        "/home/akuts/mysqlproxy/handle.pl")));
        client_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_client_query_logger(*this, "query.log")));
        client_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_statistics_handler(*this)));
        client_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_write_handler(server_output_stream_)));

        server_packet_handlers_.clear();
        server_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_server_password_interceptor(interceptor)));
        server_packet_handlers_.push_back(boost::shared_ptr<mysql_packet_handler>(
                    new mysql_write_handler(client_output_stream_)));

        compression_decision_determined_ = false;
        ssl_desicion_determined_ = false;
        in_ssl_ = false;

        init_client_read();
        init_server_read();
    }

    void set_shutdown_callback(const boost::function<void()> &cb) {
        shutdown_callback_ = cb;
    }

    bool in_command_phase() const {
        return client_state_ == CLIENT_COMMAND_PHASE
            && server_state_ == SERVER_COMMAND_PHASE;
    }

    connection_statistics &getstat() {
        return stat_;
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
        boost::shared_ptr<mysql_packet_seqid> compressed_seqid(
                new mysql_packet_seqid());

        client_input_stream_.reset(new mysql_packet_input_stream_compressed());
        client_output_stream_.reset(new mysql_packet_output_stream_compressed(
                    seqid, compressed_seqid));

        server_input_stream_.reset(new mysql_packet_input_stream_compressed());
        server_output_stream_.reset(new mysql_packet_output_stream_compressed(
                    seqid, compressed_seqid));

        client_packet_handlers_.back().reset(
                new mysql_write_handler(server_output_stream_));

        server_packet_handlers_.back().reset(
                    new mysql_write_handler(client_output_stream_));
    }

    void maybe_turnon_ssl() {
        if (ssl_desicion_determined_)
            return;

        if (client_state_ == SSL_CONNECTION_REQUEST) {
            turnon_ssl();
            ssl_desicion_determined_ = true;
        } else if (client_state_ > SSL_CONNECTION_REQUEST) {
            /* no ssl */
            ssl_desicion_determined_ = true;
        }
    }

    void turnon_ssl() {
#ifdef DEBUG
        fprintf(stderr, "Switching to ssl\n");
#endif
        client_context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);

        client_context_.use_certificate_chain_file(settings_.server_cert);
        client_context_.use_private_key_file(
            settings_.server_key, boost::asio::ssl::context::pem);
        client_context_.use_tmp_dh_file(settings_.dh_file);

        /*
        client_context_.load_verify_file(settings_.server_ca_file);
        client_context_.set_verify_mode(
                boost::asio::ssl::context_base::verify_peer); */

        /*
        server_context_.use_certificate_chain_file(settings_.client_cert);
        server_context_.use_private_key_file(
            settings_.client_key, boost::asio::ssl::context::pem);
        server_context_.use_tmp_dh_file(settings_.dh_file);
        */

        server_context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
        client_socket_ssl_stream_.reset(
            new ssl_stream_type(client_socket_, client_context_));
        server_socket_ssl_stream_.reset(
            new ssl_stream_type(server_socket_, server_context_));

        client_socket_ssl_stream_->handshake(
                boost::asio::ssl::stream_base::server);
        server_socket_ssl_stream_->handshake(
                boost::asio::ssl::stream_base::client);

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
        /* we sometimes cancel requests */
        if (error == boost::asio::error::operation_aborted) {
            return;
        }

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

        bool old_in_ssl = in_ssl_;

        maybe_turnon_ssl();

        /* turned on ssl  */
        if (!old_in_ssl && in_ssl_) {
            server_socket_.cancel();
            init_server_read(); 
        }

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
                unpack_from_packet(ssl_request, packet);
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
                unpack_from_packet(response, packet);
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
            output_stream->reset_packets();
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
            output_stream->reset_packets();
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
        /* we sometimes cancel requests */
        if (error == boost::asio::error::operation_aborted) {
            return;
        }

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
            boost::shared_ptr<packet_t> packet 
                = server_input_stream_->packet_top();
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
            unpack_from_packet(initial_handshake, packet);
    
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
        client_socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        client_socket_.close();
        server_socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        server_socket_.close();
        shutdown_callback_();
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

    boost::function<void()> shutdown_callback_;
    connection_statistics stat_;
};

void mysql_query_perl_processor::handle(packet_t *packet) {
    if (conn_.in_command_phase()) {
        enum_server_command cmd = static_cast<enum_server_command>(
            packet->get_pointer()[0]);
        /*
        if (cmd == COM_QUERY) {
            mysql_procotol_com_query_packet query;
            unpack_from_packet(query, packet);
            std::string transformed = p_.call("handle", 
                std::string(query.query_string.b, query.query_string.size));
            packet->set_content(static_cast<char>(cmd) + transformed);
        }
        */
        if (cmd == COM_QUERY) {
            mysql_procotol_com_query_packet query;
            unpack_from_packet(query, packet);
            if (query.query_string.size >= 6
                    && !strncasecmp("select", query.query_string.b, 6))
            {
                std::string query_str(query.query_string.b, 
                        query.query_string.size);
                myisamlib::QueryUnion q 
                    = myisamlib::ConstructQueryObject(query_str.c_str());

                SV *qunion = p_.transform_query_union_to_perlsv(q);
                std::string transformed = p_.call("handle", qunion);
                std::cerr << transformed << std::endl;
            }
        }
    }
}


void mysql_statistics_handler::handle(packet_t *packet) {
    if (conn_.in_command_phase()) {
        conn_.getstat().increment(static_cast<enum_server_command>(
            packet->get_pointer()[0]));
    }
}

void mysql_client_query_logger::handle(packet_t *packet) {
    if (conn_.in_command_phase()) {
        enum_server_command cmd = static_cast<enum_server_command>(
            packet->get_pointer()[0]);
        if (cmd == COM_QUERY) {
            mysql_procotol_com_query_packet query;
            unpack_from_packet(query, packet);

            if (query.query_string.size >= 6
                    && !strncasecmp("select", query.query_string.b, 6))
            {
                std::string query_str(query.query_string.b, 
                        query.query_string.size);
                myisamlib::QueryUnion q 
                    = myisamlib::ConstructQueryObject(query_str.c_str());
                std::string qstr = q.to_string();
                fs_ << q.to_string() << std::endl;
                std::cerr << q.to_string() << std::endl;
            }
        }

    }
}

void mysql_password_interceptor::reset_state() {
    client_state_ = CLIENT_NONE;
    server_state_ = SERVER_NONE;
    table_columns_count_ = 0;
    columns_to_read_ = 0;

    columns_to_intercept_indices_.clear();
    column_definitions_.clear();
}

void mysql_password_interceptor::compute_intercept_indices() {
    for (int i = 0; i < column_definitions_.size(); ++i) {
        for (int j = 0; j < columns_to_intercept_.size(); ++j) {
            if (!strcasecmp(column_definitions_[i].c_str(),
                    columns_to_intercept_[j].c_str()))
            { 
                columns_to_intercept_indices_.push_back(i); 
                break; 
            }
        }
    }
}

void mysql_password_interceptor::perform_intercept() {
    for (int i = 0; i < columns_to_intercept_indices_.size(); ++i) {
        int index = columns_to_intercept_indices_[i];
        std::cerr << column_definitions_[index] << ": `";
        std::cerr << (current_row_null_fields_[index] 
            ? "NULL" : current_row_[index]) << "'\t";
    }
    if (!columns_to_intercept_indices_.empty()) { 
        std::cerr << std::endl; 
    }
}

void mysql_password_interceptor::handle_from_client(packet_t *packet) {
    switch (client_state_) {
    case CLIENT_NONE: {
        enum_server_command cmd = static_cast<enum_server_command>(
            packet->get_pointer()[0]);
        if (cmd == COM_QUERY) { 
            client_state_ = CLIENT_QUERY_SENT; 
        }
        break;
    }

    case CLIENT_QUERY_SENT: /* do nothing */ break;
    };
}

void mysql_password_interceptor::handle_from_server(packet_t *packet) {
    if (!packet->get_size() || client_state_ != CLIENT_QUERY_SENT)
        return;

    switch (server_state_) {
    case SERVER_NONE: {
        length_encoded_integer number_of_columns;
        unpack_from_packet(number_of_columns, packet);
        table_columns_count_ = columns_to_read_ = number_of_columns.b;
        server_state_ = READING_COLUMNS; 
        break;
    }

    case READING_COLUMNS: {
        if (!columns_to_read_--) {
            /* there must be an eof packet */ 
            mysql_protocol_eof_packet eof_packet;
            unpack_from_packet(eof_packet, packet);
            server_state_ = READING_ROWS;
            compute_intercept_indices();
        } else {
            mysql_protocol_column_definition column_definition;
            unpack_from_packet(column_definition, packet);
            column_definitions_.push_back(column_definition.name.b);
        }
        break;
    }

    case READING_ROWS: {
        uchar c = static_cast<uchar>(packet->get_pointer()[0]);

        /* eof-packet */
        if (c == 0xfe) {
            mysql_protocol_eof_packet eof_packet;
            unpack_from_packet(eof_packet, packet);
            if (eof_packet.status_flags.b & SERVER_MORE_RESULTS_EXISTS) {
                server_state_ = SERVER_NONE; /* TODO client_protocol41 check */
            } else {
                reset_state();
            }
            return;
        }

        /* error-packet */
        if (c == 0xff) {
            reset_state();
            return;
        }
        
        /* reading rows from here */
        current_row_.resize(table_columns_count_);
        current_row_null_fields_.resize(table_columns_count_);

        char *p = packet->get_pointer();
        char *end = p + packet->get_size();
        for (int i = 0; i < table_columns_count_; ++i) {
            /* if is null */
            if (*p == 0xfb) {
                current_row_null_fields_[i] = true;
                p++;
            } else {
                length_encoded_string column_row_item;
                p = column_row_item.unpack(p,
                        packet->get_pointer() + packet->get_size());
                current_row_[i] = column_row_item.b;
                current_row_null_fields_[i] = false;
            }
        }

        perform_intercept();

        break;
    }
    };
}

class mysqlproxy {
public:
    enum { TICK_INTERVAL = 3 };

public:
    mysqlproxy(
        const std::string &bind_host,
        int bind_port,
        const connection_settings &settings)
    : bind_host_(bind_host),
      bind_port_(bind_port),
      settings_(settings),
      io_service_(),
      acceptor_(io_service_),
      on_tick_timer_(io_service_)
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
        new_connection_->set_shutdown_callback(
                boost::bind(&mysqlproxy::on_connection_shutdown,
                this, new_connection_));
        current_connections_.insert(new_connection_);

        start_accept();
    }

    void run() { io_service_.run(); }

private:
    void on_connection_shutdown(
        const boost::shared_ptr<mysql_connection> &connection) 
    { 
        purge_queue_.push_back(connection); 
        current_connections_.erase(connection);
    }

    void init_on_tick_interval() {
        on_tick_timer_.expires_from_now(
                boost::posix_time::seconds(TICK_INTERVAL));
        on_tick_timer_.async_wait(boost::bind(&mysqlproxy::on_tick, this,
            boost::asio::placeholders::error));
    }

    void on_tick(const boost::system::error_code& e) {
        if (e != boost::asio::error::operation_aborted) {
            std::vector<boost::shared_ptr<mysql_connection> >().swap(purge_queue_);
            init_on_tick_interval();
        }
    }

    void start_accept() {
        new_connection_.reset(new mysql_connection(settings_, io_service_));
        acceptor_.async_accept(new_connection_->client_socket(),
            boost::bind(&mysqlproxy::on_accept, this,
                boost::asio::placeholders::error));
        init_on_tick_interval();
    }

private:
    std::string bind_host_;
    int bind_port_;

    connection_settings settings_;

    boost::asio::io_service io_service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::shared_ptr<mysql_connection> new_connection_;

    std::vector<boost::shared_ptr<mysql_connection> > purge_queue_;
    std::set<boost::shared_ptr<mysql_connection> > current_connections_;

    boost::asio::basic_deadline_timer<boost::posix_time::ptime>
        on_tick_timer_;
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
    myisamlib::MyIsamLib library;

    const char *testdir_path 
        = "/home/akuts/stuff/experiments/mysql-5.1.65/build/datadir/test";
    boost::filesystem::remove_all(library.tmpdirname() 
            + "/support_files/datadir/test");
    boost::filesystem::create_symlink(testdir_path,
        library.tmpdirname() + "/support_files/datadir/test");

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

