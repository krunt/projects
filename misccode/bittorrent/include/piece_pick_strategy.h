#ifndef PIECE_PICK_STRATEGY_DEF_
#define PIECE_PICK_STRATEGY_DEF_

namespace btorrent {

/* this strategy is only working with active peers */
class piece_pick_strategy_t {
public:
    virtual bool get_next_part(piece_part_request_t &request) = 0;

    virtual void on_peer_added(const peer_piece_bitmap_t &bitmap) = 0;
    virtual void on_peer_removed(const peer_piece_bitmap_t &bitmap) = 0;
    virtual void on_piece_part_aborted(const piece_part_request_t &req) = 0;
    virtual void on_piece_part_requested(const piece_part_request_t &req) = 0;
    virtual void on_piece_part_downloaded(const piece_part_request_t &req) = 0;

    virtual void on_piece_done(size_type piece_index) = 0;
    virtual void on_piece_validation_failed(size_type piece_index) = 0;
};

}

#endif /* PIECE_PICK_STRATEGY_DEF_ */
