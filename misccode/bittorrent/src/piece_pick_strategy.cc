#include <include/common.h>
#include <include/most_rarest_piece_pick_strategy.h>

namespace btorrent {

piece_pick_strategy_t *create_piece_pick_strategy(
    piece_pick_strategy::k_piece_pick_strategy_type type, 
    const peer_piece_bitmap_t &my_bitmap) 
{
    if (type == piece_pick_strategy::most_rarest) {
        return new most_rarest_piece_pick_strategy_t(my_bitmap);
    }
    return NULL;
}

}
