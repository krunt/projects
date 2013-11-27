#include <include/torrent_storage.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace btorrent {

torrent_storage_t::torrent_storage_t(torrent_t &torrent)
    : m_piece_part_size(gsettings()->m_piece_part_size),
      m_piece_size(torrent.get_torrent_info().m_piece_size)
{}

void torrent_storage_t::preallocate_file(const file_stream_t &f) {
    fs::path pth(f.m_ft.m_path);
    fs::create_directories(pth.branch_path());
    f.m_stream.seek(f.m_size, std::ios_base::beg);
    f.m_stream.close();
    f.m_stream.open(m_ft.m_path.c_str(), std::ios_base::out)
}

void torrent_storage_t::setup_files() {
    const torrent_info_t &info = m_torrent.get_torrent_info();

    m_accumulated_file_sizes.push_back(0);

    /* make paths absolute */
    for (int i = 0; i < info.m_files.size(); ++i) {
        info.m_files[i].m_path = gsettings()->m_torrents_data_path
            / fs::path(info.m_files[i].m_path);

        m_files.push_back(file_stream_t(info.m_files[i]));
        preallocate_file(m_files.back());

        m_accumulated_file_sizes.push_back(
            m_accumulated_file_sizes.back() + m_files.back().m_ft.m_size);
    }
}

void torrent_storage_t::add_piece_part(int piece_index, 
        int piece_part, const vector<u8> &part_data)
{
    assert(part_data.size() == m_piece_part_size);

    file_stream_iterator_t it(
            construct_file_stream_iterator(piece_index, piece_part));

    size_type bytes_left = m_piece_part_size;
    for (; !it.at_end(); ++it) {
        const file_stream_iterator_element_t &el = *it;
        el.stream->seek(el.offset, std::ios_base::beg);
        el.stream->write(&part_data[m_piece_part_size - bytes_left], el.size);
        bytes_left -= el.size;
    }
    assert(bytes_left == 0);
}

bool torrent_storage_t::validate_piece(int piece_index) {
    std::vector<u8> piece_buf;
    sha1_hash_t piece_hash;
    piece_hash.init();

    file_stream_iterator_t it(
        construct_file_stream_iterator(piece_index, 0, m_piece_size));
    for (; !it.at_end(); ++it) {
        const file_stream_iterator_element_t &el = *it;
        el.stream->seek(el.offset, std::ios_base::beg);

        /* maybe do here in loop */
        piece_buf.resize(el.size);
        el.stream->read(&piece_buf[0], el.size);
        piece_hash.update(piece_buf);
    }

    piece_hash.finalize();
    return piece_hash == m_torrent.get_torrent_info().m_piece_hashes[piece_index];
}

torrent_storage_t::file_stream_iterator_t construct_file_stream_iterator(
    size_type piece_index, size_type piece_part, size_type bytes_to_read) 
{
    return file_stream_iterator_t(m_files, m_accumulated_file_sizes,
            piece_index, piece_part, m_piece_size, m_piece_part_size,
            bytes_to_read == 0 ? m_piece_part_size : bytes_to_read);
}

torrent_storage_t::file_stream_iterator_t::file_stream_iterator_t(
        const std::vector<file_stream_t> &files,
        const std::vector<size_type> &accumulated_file_sizes,
        size_type piece_index, size_type piece_part,
        size_type piece_size, size_type piece_part_size, 
        size_type bytes_to_read) 
    : m_files(files), m_at_end(false),
      m_left_bytes(bytes_to_read)
{
    m_file_offset = piece_index * piece_size + piece_part * piece_part_size;
    std::vector<size_type>::const_iterator it
        = std::lower_bound(accumulated_file_sizes.begin(),
            accumulated_file_sizes.end(), m_file_offset);

    /* step back a little */
    if (it != accumulated_file_sizes.begin() && (*it) != m_file_offset)
        --it;

    m_global_offset = m_file_offset;
    m_file_offset -= *it;
    m_file_index = std::distance(m_accumulated_file_sizes.begin(), it);
}

const torrent_storage_t::file_stream_iterator_element_t
torrent_storage_t::file_stream_iterator_t::operator*() {

    assert(m_file_index + 1 < m_accumulated_file_sizes.size());

    file_stream_iterator_element_t result;
    result.stream = &m_files[m_file_index];
    result.offset = m_file_offset;
    result.size = std::min(m_left_bytes,
        m_accumulated_file_sizes[m_file_index + 1] - m_global_offset);

    return result;
}

torrent_storage_t::file_stream_iterator_t &
torrent_storage_t::file_stream_iterator_t::operator++() {

    size_type bytes_to_write = std::min(m_left_bytes,
        m_accumulated_file_sizes[m_file_index + 1] - m_global_offset);

    if (!(m_left_bytes -= bytes_to_write)) {
        m_at_end = true;
    } else {
        m_global_offset += bytes_to_write;
        m_file_index++;
        m_file_offset = 0;
    }

    return *this;
}

}
