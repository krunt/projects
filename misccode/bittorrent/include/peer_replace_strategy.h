#ifndef PEER_REPLACE_STRATEGY_DEF_
#define PEER_REPLACE_STRATEGY_DEF_

namespace btorrent {

namespace peer_replace_strategy {
enum k_peer_replace_strategy_type { general, };
}

class peer_replace_strategy_t {
public:
    /* get replacement recommendation from active to inactive */
    virtual void get_peer_replacements(
        std::vector<peer_replacement_t> &replacements) = 0;

    /* adding to active */
    virtual bool get_peer_to_add(std::string &peer_id) = 0;

    /* removing from active */
    virtual void get_peers_to_remove(std::vector<std::string> &peer_ids) = 0;

    virtual void on_active_peer_added(const peer_piece_bitmap_t &bitmap) = 0;
    virtual void on_inactive_peer_added(const peer_piece_bitmap_t &bitmap) = 0;

    virtual void on_active_peer_removed(const peer_piece_bitmap_t &bitmap) = 0;
    virtual void on_inactive_peer_removed(const peer_piece_bitmap_t &bitmap) = 0;

    virtual void on_piece_part_aborted(const piece_part_request_t &req) = 0;
    virtual void on_piece_part_requested(const piece_part_request_t &req) = 0;
    virtual void on_piece_part_downloaded(const piece_part_request_t &req) = 0;

    virtual void on_piece_validation_done(size_type piece_index) = 0;
    virtual void on_piece_validation_failed(size_type piece_index) = 0;
};

peer_replace_strategy_t *create_peer_replace_strategy(
    peer_replace_strategy::k_peer_replace_strategy_type type, 
    const peer_piece_bitmap_t &my_bitmap);

}

#endif /* PEER_REPLACE_STRATEGY_DEF_ */
