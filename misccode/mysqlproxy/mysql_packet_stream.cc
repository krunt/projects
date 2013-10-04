#define MAX_PACKET_LENGTH (256L*256L*256L-1)

class mysql_packet_output_stream {
public:
    mysql_packet_input_stream() 
        : output_packet_count(0), 
          output_packet_fragments_count(0),
          output_seq(0) 
    { init_dynamic_string(&output, "", 128, 16); }

    ~mysql_packet_input_stream() { 
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

class mysql_packet_output_stream_compressed {
public:
    mysql_packet_input_stream_compressed() 
        : output_packet_count(0), 
          output_packet_fragments_count(0),
          output_seq(0),
          compressed_output_seq(0)
    { init_dynamic_string(&output, "", 128, 16); }

    ~mysql_packet_input_stream_compressed() { 
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

/* TODO: leaking memory when freeing list */
class mysql_packet_input_stream {
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


class mysql_packet_input_stream_compressed {
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

