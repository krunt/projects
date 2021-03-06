#include <include/common.h>

namespace btorrent {

peer_piece_bitmap_t::peer_piece_bitmap_t(const std::string &peer_id,
        size_type piece_count, size_type parts_per_piece) 
    : m_peer_id(peer_id), m_piece_count(piece_count),
      m_parts_per_piece(parts_per_piece)
{
    m_piece_bitmap.resize((piece_count + 7) >> 3, 0);
    m_piece_part_bitmap.resize(
        ((m_piece_bitmap.size() * 8) * parts_per_piece * 2 + 7) >> 3, 0);
}

peer_piece_bitmap_t::peer_piece_bitmap_t(const std::string &peer_id,
        const std::vector<u8> &bitmap, size_type parts_per_piece)
    : m_peer_id(peer_id), m_piece_bitmap(bitmap), 
      m_piece_count(bitmap.size() * 8), m_parts_per_piece(parts_per_piece)
{
    m_piece_part_bitmap.resize(
        ((m_piece_bitmap.size() * 8) * parts_per_piece * 2 + 7) >> 3, 0);
    for (int i = 0; i < m_piece_bitmap.size(); ++i) {
        for (int j = 0; j < 8; ++j) {
            if (m_piece_bitmap[i] & (1 << j)) {
                set_piece(i * 8 + j);
            }
        }
    }
}

/* position in bits */
size_type peer_piece_bitmap_t::get_part_index(size_type piece_index,
        size_type piece_part_index) const
{
    return (piece_index * m_parts_per_piece * 2 + piece_part_index * 2);
}

peer_piece_bitmap_t::piece_part_state_t 
peer_piece_bitmap_t::get_piece_part(size_type piece_index,
        size_type piece_part_index) const
{
    size_type part_index = get_part_index(piece_index, piece_part_index);
    piece_part_state_t result = static_cast<piece_part_state_t>(
        (m_piece_part_bitmap[part_index >> 3] & (3 << (part_index & 7)))
        >> (part_index & 7));
    return result;
}

void peer_piece_bitmap_t::set_piece_part(size_type piece_index,
        size_type piece_part_index, piece_part_state_t state)
{
    size_type part_index = get_part_index(piece_index, piece_part_index);
    m_piece_part_bitmap[part_index >> 3] &= ~(3 << (part_index & 7));
    m_piece_part_bitmap[part_index >> 3] |= (int)state << (part_index & 7);
}

void peer_piece_bitmap_t::set_piece(size_type piece_index) {
    m_piece_bitmap[piece_index >> 3] |= 1 << (piece_index & 7);

    for (int part_num = 0; part_num < m_parts_per_piece; ++part_num) {
        set_piece_part(piece_index, part_num, pp_done);
    }
}

void peer_piece_bitmap_t::unset_piece(size_type piece_index) {
    m_piece_bitmap[piece_index >> 3] &= ~(1 << (piece_index & 7));

    for (int part_num = 0; part_num < m_parts_per_piece; ++part_num) {
        set_piece_part(piece_index, part_num, pp_none);
    }
}

bool peer_piece_bitmap_t::has_piece(size_type piece_index) const {
    return m_piece_bitmap[piece_index >> 3] & (1 << (piece_index & 7));
}

bool peer_piece_bitmap_t::is_piece_downloaded(size_type piece_index) const {
    for (int part_num = 0; part_num < m_parts_per_piece; ++part_num) {
        if (get_piece_part(piece_index, part_num) != pp_downloaded)
            return false;
    }
    return true;
}

DEFINE_CONST_METHOD(bool, peer_piece_bitmap_t::is_done)
    GLOG->debug("checking %d pieces for completion", piece_count());
    for (int i = 0; i < piece_count(); ++i) {
        if (!has_piece(i)) {
            GLOG->debug("%d piece is not done yet", i);
            return false;
        }
    }
    return true;
END_METHOD

size_type peer_piece_bitmap_t::piece_done_count() const {
    size_type res = 0;
    for (int i = 0; i < piece_count(); ++i) {
        if (has_piece(i))
            res++;
    }
    return res;
}

std::string peer_piece_bitmap_t::to_string() const {
    std::string res = "";
    for (int i = 0; i < m_piece_bitmap.size(); ++i) {
        for (int j = 0; j < 8; ++j) {
            res += m_piece_bitmap[i] & (1 << j) ? "1" : "0";
        }
    }
    return res;
}

peer_piece_bitmap_iterator_t::peer_piece_bitmap_iterator_t(
        const peer_piece_bitmap_t &bitmap, int filter_match)
    : m_filter_match(filter_match), m_pos(-1), m_at_end(false),
      m_bitmap(bitmap)
{ operator++(); }

peer_piece_bitmap_iterator_t::value_type
peer_piece_bitmap_iterator_t::operator*() const {
    return m_pos; 
}

peer_piece_bitmap_iterator_t &
peer_piece_bitmap_iterator_t::operator++() {
    /* glog()->debug("%d/%d/%d", m_pos, m_bitmap.piece_count(),
            m_bitmap.has_piece(m_pos)); */
    while (1) {
        if (++m_pos >= m_bitmap.piece_count()) {
            m_at_end = true;
            break;
        }

        if (m_filter_match == -1 
            || (m_filter_match == 0 && !m_bitmap.has_piece(m_pos))
            || (m_filter_match == 1 && m_bitmap.has_piece(m_pos)))
        {
            break;
        }
    }
    return *this;
}

peer_piece_part_bitmap_iterator_t::peer_piece_part_bitmap_iterator_t(
        const peer_piece_bitmap_t &bitmap, 
        size_type piece_index, int filter_match)
    : m_filter_match(filter_match), m_piece_index(piece_index), 
    m_pos(-1), m_at_end(false), m_bitmap(bitmap)
{ operator++(); }

peer_piece_part_bitmap_iterator_t::value_type
peer_piece_part_bitmap_iterator_t::operator*() const {
    return m_pos; 
}

peer_piece_part_bitmap_iterator_t &
peer_piece_part_bitmap_iterator_t::operator++() {
    while (1) {
        if (++m_pos >= m_bitmap.parts_per_piece()) {
            m_at_end = true;
            break;
        }

        if (m_filter_match == -1 
            || (m_bitmap.get_piece_part(m_piece_index, m_pos) == m_filter_match))
        {
            break;
        }
    }
    return *this;
}

}
