#ifndef PIECE_PICK_STRATEGY_DEF_
#define PIECE_PICK_STRATEGY_DEF_

#include <include/common.h>

namespace btorrent {

namespace piece_pick_strategy {
enum k_piece_pick_strategy_type { most_rarest, };
}

/* this strategy is only working with active peers */
class piece_pick_strategy_t {
public:
    virtual bool get_next_part(piece_part_request_t &request) = 0;

    virtual void on_peer_added(const peer_piece_bitmap_t &bitmap) = 0;
    virtual void on_peer_removed(const peer_piece_bitmap_t &bitmap) = 0;
    virtual void on_piece_part_aborted(const piece_part_request_t &req) = 0;
    virtual void on_piece_part_requested(const piece_part_request_t &req) = 0;
    virtual void on_piece_part_downloaded(const piece_part_request_t &req) = 0;

    virtual void on_piece_validation_done(size_type piece_index) = 0;
    virtual void on_piece_validation_failed(size_type piece_index) = 0;
};

piece_pick_strategy_t *create_piece_pick_strategy(
    piece_pick_strategy::k_piece_pick_strategy_type type, 
    const peer_piece_bitmap_t &my_bitmap);

}

#endif /* PIECE_PICK_STRATEGY_DEF_ */
