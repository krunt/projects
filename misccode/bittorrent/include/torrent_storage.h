#ifndef TORRENT_STORAGE_DEF_
#define TORRENT_STORAGE_DEF_

#include <include/common.h>
#include <fstream>

namespace btorrent {

class torrent_storage_t {
public:
    torrent_storage_t(torrent_t &torrent);

    void start();
    void finish();

    void add_piece_part(int piece_index, int piece_part, const vector<u8> &data);
    bool validate_piece(int piece_index) const;

    torrent_t &get_torrent() { return m_torrent; }
    const torrent_t &get_torrent() const { return m_torrent; }

private:
    void setup_files();
    void preallocate_file(const file_stream_t &f);

private:
    torrent_t &m_torrent;

    struct file_stream_t {
        file_stream_t(const torrent_info_t::file_t &ft)
            : m_ft(ft)
        { m_stream.open(m_ft.m_path.c_str(), std::ios_base::out); }

        torrent_info_t::file_t m_ft;
        std::fstream m_stream;
    };

    struct file_stream_iterator_element_t {
        file_stream_t *stream;
        size_type offset;
        size_type size;
    };

    class file_stream_iterator_t: public
        std::iterator<std::input_iterator_tag, file_stream_iterator_element_t> {
    public:
        file_stream_iterator_element_t(int piece_index, int piece_part);

        const file_stream_iterator_element_t operator*() const;
        file_stream_iterator_t &operator++();

        bool at_end() const { return m_at_end; }

    private:
        size_type convert_piece_part_to_offset(
                size_type piece_index, size_type piece_part) const {
            return piece_index * m_piece_size + piece_part * m_piece_part_size;
        }

        const std::vector<file_stream_t> &m_files;
        const std::vector<size_type> &m_accumulated_file_sizes,

        bool m_at_end;
        size_type m_global_offset;
        size_type m_piece_part_offset;
        size_type m_left_bytes;
        int m_file_index;
    };

    file_stream_iterator_t construct_file_stream_iterator(
        size_type piece_index, size_type piece_part, size_type bytes_to_read = 0);

private:
    size_type m_piece_part_size;
    size_type m_piece_size;

    std::vector<file_stream_t> m_files;
    std::vector<size_type> m_accumulated_file_sizes;
};

}


#endif /* TORRENT_STORAGE_DEF_ */
