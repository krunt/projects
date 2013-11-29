namespace btorrent {

peer_replace_strategy_t *create_peer_replace_strategy(
    k_peer_replace_strategy_type  type, const peer_piece_bitmap_t &my_bitmap) 
{
    if (type == peer_replace_strategy::general) {
        return new most_rarest_piece_pick_strategy_t(my_bitmap);
    }
    return NULL;
}

}
