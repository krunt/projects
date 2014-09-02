#include <include/torrent_storage.h>
#include <boost/filesystem.hpp>

#include <include/torrent.h>

namespace fs = boost::filesystem;

namespace btorrent {

torrent_storage_t::torrent_storage_t(torrent_t &torrent)
    : m_torrent(torrent),
      m_piece_part_size(gsettings()->m_piece_part_size),
      m_piece_size(torrent.get_torrent_info().m_piece_size)
{}

void torrent_storage_t::start() { 
    setup_files();
}

void torrent_storage_t::finish() { 
}

DEFINE_METHOD(void, torrent_storage_t::dispatch_requests_queue)
    while (true) {
        torrent_storage_work_t work = m_requests.pop();

        GLOG->debug("got type %d work", work.m_type);

        switch (work.m_type) {
        case torrent_storage_work_t::k_add_piece_part:
            add_piece_part(work.m_piece_index, work.m_piece_part_index,
                    work.m_data);
        break;

        case torrent_storage_work_t::k_get_piece_part:
            get_piece_part(work.m_piece_index, work.m_piece_part_index,
                    work.m_data);
        break;

        case torrent_storage_work_t::k_validate_piece:
            work.m_validation_result = validate_piece(work.m_piece_index);
        break;

        default:
            assert(0);
        };

        m_torrent.on_torrent_storage_work_done(work);
    }
END_METHOD

void torrent_storage_t::preallocate_file(file_stream_t &f) {
    f.m_stream->close();
    f.m_stream->open(f.m_ft.m_path.c_str(), 
        std::ios_base::out | std::ios_base::binary);
    f.m_stream->seekp(f.m_ft.m_size - 1, std::ios_base::beg);
    f.m_stream->write("\0", 1);
    f.m_stream->close();
    f.m_stream->open(f.m_ft.m_path.c_str(), 
        std::ios_base::in | std::ios_base::out | std::ios_base::binary);
}

DEFINE_METHOD(void, torrent_storage_t::setup_files)
    const torrent_info_t &info = m_torrent.get_torrent_info();

    m_accumulated_file_sizes.push_back(0);

    /* make paths absolute */
    for (int i = 0; i < info.m_files.size(); ++i) {
        fs::path data_path = fs::path(gsettings()->m_torrents_data_path);
        data_path /= info.m_files[i].m_path;

        torrent_info_t::file_t fcopy = info.m_files[i];
        fcopy.m_path = data_path.string();

        GLOG->debug("preallocating file %s", fcopy.m_path.c_str());

        fs::create_directories(fs::path(fcopy.m_path).branch_path());
        m_files.push_back(file_stream_t(fcopy));
        preallocate_file(m_files.back());

        m_accumulated_file_sizes.push_back(
            m_accumulated_file_sizes.back() + m_files.back().m_ft.m_size);
    }
END_METHOD


void torrent_storage_t::add_piece_part_enqueue_request(ppeer_t peer, 
        int piece_index, int piece_part, const std::vector<u8> &part_data)
{
    m_requests.push(torrent_storage_work_t(
            torrent_storage_work_t::k_add_piece_part, peer,
            piece_index, piece_part, part_data));
}

void torrent_storage_t::add_piece_part(int piece_index, 
        int piece_part, const std::vector<u8> &part_data)
{
    assert(part_data.size() == m_piece_part_size);

    file_stream_iterator_t it(
            construct_file_stream_iterator(piece_index, piece_part));

    size_type bytes_left = m_piece_part_size;
    for (; !it.at_end(); ++it) {
        const file_stream_iterator_element_t &el = *it;
        el.stream->m_stream->seekp(el.offset, std::ios_base::beg);
        el.stream->m_stream->write(
            (const char *)&part_data[m_piece_part_size - bytes_left], el.size);
        el.stream->m_stream->flush();
        bytes_left -= el.size;
    }
}

void torrent_storage_t::get_piece_part_enqueue_request(ppeer_t peer, 
        int piece_index, int piece_part, std::vector<u8> &part_data)
{
    m_requests.push(torrent_storage_work_t(
            torrent_storage_work_t::k_get_piece_part, peer,
            piece_index, piece_part, part_data));
}

void torrent_storage_t::get_piece_part(int piece_index, int piece_part_index,
        std::vector<u8> &data)
{
    int prev_size;
    file_stream_iterator_t it(
            construct_file_stream_iterator(piece_index, piece_part_index,
                gsettings()->m_piece_part_size));

    for (; !it.at_end(); ++it) {
        const file_stream_iterator_element_t &el = *it;

        el.stream->m_stream->seekp(el.offset, std::ios_base::beg);

        /* maybe do here in loop */
        prev_size = data.size();
        data.resize(prev_size + el.size);
        el.stream->m_stream->read((char *)&data[prev_size], el.size);

        if (el.stream->m_stream->fail()) {
            throw std::runtime_error("error while reading");
        }
    }
}

void torrent_storage_t::validate_piece_enqueue_request(ppeer_t peer,
        int piece_index) 
{
    m_requests.push(torrent_storage_work_t(
            torrent_storage_work_t::k_validate_piece, 
            peer, piece_index));
}

bool torrent_storage_t::validate_piece(int piece_index) const
{
    size_type bytes_read;
    std::vector<u8> piece_buf;
    sha1_hash_t piece_hash;
    piece_hash.init();

    bytes_read = 0;
    file_stream_iterator_t it(
        construct_file_stream_iterator(piece_index, 0, m_piece_size));
    for (; !it.at_end(); ++it) {
        const file_stream_iterator_element_t &el = *it;

        el.stream->m_stream->seekp(el.offset, std::ios_base::beg);

        /* maybe do here in loop */
        piece_buf.resize(el.size);
        el.stream->m_stream->read((char *)&piece_buf[0], el.size);

        bytes_read += el.size;

        if (el.stream->m_stream->fail()) {
            throw std::runtime_error("error while reading");
        }

        piece_hash.update((const char *)piece_buf.data(), piece_buf.size());
    }

    /* not full piece-read, fill with zero bytes, 
     * can happen at file-end only */
    if ((bytes_read % m_piece_size) != 0) {
        size_type to_read = m_piece_size - (bytes_read % m_piece_size);
        for (int i = 0; i < to_read; ++i) {
            piece_hash.update("\0", 1);
        }
    }

    piece_hash.finalize();

    if (piece_hash != m_torrent.get_torrent_info().m_piece_hashes[piece_index]) {
        glog()->debug("validation mismatch " + hex_encode(piece_hash.get_digest())
        + " != " + hex_encode(m_torrent.get_torrent_info()
            .m_piece_hashes[piece_index].get_digest()));
    }

    return piece_hash == m_torrent.get_torrent_info().m_piece_hashes[piece_index];
}


torrent_storage_t::file_stream_iterator_t 
torrent_storage_t::construct_file_stream_iterator(
    size_type piece_index, size_type piece_part, size_type bytes_to_read) const
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
    : m_files(files), m_accumulated_file_sizes(accumulated_file_sizes),
      m_at_end(false), m_left_bytes(bytes_to_read)
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

    if (m_file_index + 1 >= m_accumulated_file_sizes.size()) {
        m_at_end = true;
    }
}

const torrent_storage_t::file_stream_iterator_element_t
torrent_storage_t::file_stream_iterator_t::operator*() const {

    assert(m_file_index + 1 < m_accumulated_file_sizes.size());

    file_stream_iterator_element_t result;
    result.stream = (file_stream_t *)&m_files[m_file_index];
    result.offset = m_file_offset;
    result.size = std::min(m_left_bytes,
        m_accumulated_file_sizes[m_file_index + 1] - m_global_offset);

    return result;
}

torrent_storage_t::file_stream_iterator_t &
torrent_storage_t::file_stream_iterator_t::operator++() {

    size_type bytes_to_write = std::min(m_left_bytes,
        m_accumulated_file_sizes[m_file_index + 1] - m_global_offset);

    if (!(m_left_bytes -= bytes_to_write) 
        || m_file_index + 2 == m_accumulated_file_sizes.size())
    {
        m_at_end = true;
    } else {
        m_global_offset += bytes_to_write;
        m_file_index++;
        m_file_offset = 0;
    }

    return *this;
}

}
