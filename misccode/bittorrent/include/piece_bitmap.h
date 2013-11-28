#ifndef PIECE_PICK_STRATEGY_DEF_
#define PIECE_PICK_STRATEGY_DEF_

namespace btorrent {

class peer_piece_bitmap_t {
public: 
    peer_piece_bitmap_t(const std::string &peer_id);
    peer_piece_bitmap_t(const std::string &peer_id, const std::vector<u8> &bitmap);

    enum piece_part_state_t {
        pp_none,
        pp_requested,
        pp_downloaded,
        pp_done,
    };

    void set_piece(size_type piece_index);
    void unset_piece(size_type piece_index);

    void set_piece_part(size_type piece_index, 
        size_type piece_part_index, piece_part_state_t state);

private:
    std::string m_peer_id;
    std::vector<u8> m_piece_part_bitmap;
    std::vector<u8> m_piece_bitmap;
};

}

#endif /* PIECE_PICK_STRATEGY_DEF_ */
