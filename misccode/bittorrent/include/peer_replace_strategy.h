#ifndef PEER_REPLACE_STRATEGY_DEF_
#define PEER_REPLACE_STRATEGY_DEF_

namespace btorrent {

class peer_replace_strategy_t {
public:

    /* get replacement recommendation from active to inactive */
    virtual bool get_peer_replacement(std::string &from_peer_id, 
           std::string &to_peer_id) = 0;

    /* adding to active */
    virtual bool get_peer_to_add(std::string &peer_id) = 0;

    /* removing from active */
    virtual bool get_peer_to_remove(std::string &peer_id) = 0;

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

}

#endif /* PEER_REPLACE_STRATEGY_DEF_ */
