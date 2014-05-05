#ifndef PEER_PIECE_BITMAP_DEF_
#define PEER_PIECE_BITMAP_DEF_

namespace btorrent {

class peer_piece_bitmap_t {
public: 
    peer_piece_bitmap_t() {}
    peer_piece_bitmap_t(const std::string &peer_id, 
        size_type piece_count, size_type parts_per_piece);
    peer_piece_bitmap_t(const std::string &peer_id, const std::vector<u8> &bitmap, 
        size_type parts_per_piece);

    enum piece_part_state_t {
        pp_none,
        pp_requested,
        pp_downloaded,
        pp_done,
    };

    void set_piece(size_type piece_index);
    void unset_piece(size_type piece_index);
    bool has_piece(size_type piece_index) const;

    void set_piece_part(size_type piece_index, 
        size_type piece_part_index, piece_part_state_t state);
    piece_part_state_t get_piece_part(size_type piece_index, 
        size_type piece_part_index) const;

    bool is_piece_downloaded(size_type piece_index) const;
    bool is_done() const;

    const std::string peer_id() const { return m_peer_id; }
    size_type piece_count() const { return m_piece_count; }
    size_type piece_done_count() const;
    size_type parts_per_piece() const { return m_parts_per_piece; }

    std::string to_string() const;

private:
    size_type get_part_index(size_type piece_index,
            size_type piece_part_index) const;

    friend class peer_piece_bitmap_iterator_t;
    friend class peer_piece_part_bitmap_iterator_t;

private:
    std::string m_peer_id;
    std::vector<u8> m_piece_part_bitmap;
    std::vector<u8> m_piece_bitmap;
    size_type m_parts_per_piece;
    size_type m_piece_count;
};

class peer_piece_bitmap_iterator_t: public 
    std::iterator<std::input_iterator_tag, size_type> 
{
public:
    peer_piece_bitmap_iterator_t(const peer_piece_bitmap_t &bitmap,
        int filter_match = -1);
    value_type operator*() const;
    peer_piece_bitmap_iterator_t &operator++();
    bool at_end() const { return m_at_end; }

private:
    const int m_filter_match;
    int m_pos; 
    bool m_at_end;
    peer_piece_bitmap_t m_bitmap;
};


class peer_piece_part_bitmap_iterator_t: public 
    std::iterator<std::input_iterator_tag, size_type> 
{
public:
    peer_piece_part_bitmap_iterator_t(const peer_piece_bitmap_t &bitmap,
        size_type piece_index, int filter_match = -1);
    value_type operator*() const;
    peer_piece_part_bitmap_iterator_t &operator++();
    bool at_end() const { return m_at_end; }

private:
    const int m_filter_match;
    const size_type m_piece_index;
    int m_pos; 
    bool m_at_end;
    peer_piece_bitmap_t m_bitmap;
};

}

#endif /* PEER_PIECE_BITMAP_DEF_ */
