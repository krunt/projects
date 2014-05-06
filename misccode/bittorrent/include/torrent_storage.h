#ifndef TORRENT_STORAGE_DEF_
#define TORRENT_STORAGE_DEF_

#include <include/common.h>
#include <include/blocking_queue.h>
#include <fstream>

namespace btorrent {

class torrent_storage_t {
public:
    torrent_storage_t(torrent_t &torrent);

    void start();
    void finish();
    void dispatch_requests_queue();

    void add_piece_part(int piece_index, int piece_part, 
            const std::vector<u8> &data);
    void get_piece_part(int piece_index, int piece_part, std::vector<u8> &data);
    bool validate_piece(int piece_index) const;

    void add_piece_part_enqueue_request(ppeer_t peer, int piece_index, int piece_part, 
            const std::vector<u8> &data);
    void get_piece_part_enqueue_request(ppeer_t peer, int piece_index, 
            int piece_part, std::vector<u8> &data);
    void validate_piece_enqueue_request(ppeer_t peer, int piece_index);

    torrent_t &get_torrent() { return m_torrent; }
    const torrent_t &get_torrent() const { return m_torrent; }

public:
    struct torrent_storage_work_t {
        enum t_type { k_add_piece_part, k_get_piece_part, k_validate_piece };

        torrent_storage_work_t() {}

        torrent_storage_work_t(t_type t, ppeer_t peer, int piece_index) 
            : m_type(t), m_piece_index(piece_index), m_peer(peer),
            m_validation_result(false)
        {}

        torrent_storage_work_t(t_type t, ppeer_t peer, 
                int piece_index, int piece_part_index,
                const std::vector<u8> &data)
            : m_type(t), m_piece_index(piece_index), 
            m_piece_part_index(piece_part_index), m_data(data),
            m_peer(peer), m_validation_result(false)
        {}

        t_type m_type;
        int m_piece_index;
        int m_piece_part_index;
        std::vector<u8> m_data;
        ppeer_t m_peer;
        bool m_validation_result;
    };

private:
    void setup_files();

private:
    torrent_t &m_torrent;

    struct file_stream_t {
        file_stream_t(const torrent_info_t::file_t &ft)
            : m_ft(ft), 
            m_stream(new std::fstream(m_ft.m_path.c_str(), 
                std::ios_base::in | std::ios_base::out | std::ios_base::binary))
        {}

        torrent_info_t::file_t m_ft;
        boost::shared_ptr<std::fstream> m_stream;
    };

    void preallocate_file(file_stream_t &f);

    struct file_stream_iterator_element_t {
        file_stream_t *stream;
        size_type offset;
        size_type size;
    };

    class file_stream_iterator_t: public
        std::iterator<std::input_iterator_tag, file_stream_iterator_element_t> {
    public:
        file_stream_iterator_t(
            const std::vector<file_stream_t> &files,
            const std::vector<size_type> &accumulated_file_sizes,
            size_type piece_index, size_type piece_part,
            size_type piece_size, size_type piece_part_size, 
            size_type bytes_to_read);

        const file_stream_iterator_element_t operator*() const;
        file_stream_iterator_t &operator++();

        bool at_end() const { return m_at_end; }

    private:
        /*
        size_type convert_piece_part_to_offset(
                size_type piece_index, size_type piece_part) const {
            return piece_index * m_piece_size + piece_part * m_piece_part_size;
        }
        */

        const std::vector<file_stream_t> &m_files;
        const std::vector<size_type> &m_accumulated_file_sizes;

        bool m_at_end;
        size_type m_global_offset;
        size_type m_piece_part_offset;
        size_type m_left_bytes;
        int m_file_index;
        int m_file_offset;
    };

    file_stream_iterator_t construct_file_stream_iterator(
        size_type piece_index, 
        size_type piece_part, size_type bytes_to_read = 0) const;

private:
    size_type m_piece_part_size;
    size_type m_piece_size;

    std::vector<file_stream_t> m_files;
    std::vector<size_type> m_accumulated_file_sizes;

    blocking_queue_t<torrent_storage_work_t, 10> m_requests;
};

}


#endif /* TORRENT_STORAGE_DEF_ */
