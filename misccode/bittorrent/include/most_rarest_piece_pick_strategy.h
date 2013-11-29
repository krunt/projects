#ifndef MOST_RAREST_PIECE_PICK_STRATEGY_DEF_
#define MOST_RAREST_PIECE_PICK_STRATEGY_DEF_

namespace btorrent {

class most_rarest_piece_pick_strategy_t: public piece_pick_strategy_t {
public:
    most_rarest_piece_pick_strategy_t(const peer_piece_bitmap_t &my_bitmap);

    virtual bool get_next_part(piece_part_request_t &request);

    virtual void on_peer_added(const peer_piece_bitmap_t &bitmap);
    virtual void on_peer_removed(const peer_piece_bitmap_t &bitmap);
    virtual void on_piece_part_aborted(const piece_part_request_t &req);
    virtual void on_piece_part_requested(const piece_part_request_t &req);
    virtual void on_piece_part_downloaded(const piece_part_request_t &req);

    virtual void on_piece_validation_done(size_type piece_index);
    virtual void on_piece_validation_failed(size_type piece_index);

private:
    struct peer_state_t {
        peer_state_t(const piece_part_bitmap_t &bitmap)
            : m_pending_count(0), m_bitmap(bitmap)
        {}

        int m_pending_count; /* pending requests count */
        piece_part_bitmap_t m_bitmap;
    };

    struct piece_sorter_t {
        piece_sorter_t(const std::vector<int> &piece_count)
            : m_piece_count(piece_count)
        {}

        bool operator()(size_type lhs_piece_index, 
                size_type rhs_piece_index) const 
        {
            return m_piece_count[lhs_piece_index]
                <  m_piece_count[rhs_piece_index];
        }

        const std::vector<int> &m_piece_count;
    };

    typedef peer_state_t *ppeer_state_t;
    int m_max_pending_requests_per_peer;

    peer_piece_bitmap_t m_mybitmap;
    std::map<std::string, peer_state_t> m_peer_map;
    std::vector<int> m_cumulative_piece_count;

    typedef std::set<ppeer_state_t> peer_list_per_piece_t;
    std::vector<peer_list_per_piece_t> m_peer_per_piece_list;
};

}

#endif /* MOST_RAREST_PIECE_PICK_STRATEGY_DEF_ */
