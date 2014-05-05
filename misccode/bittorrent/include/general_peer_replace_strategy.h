#ifndef GENERAL_PEER_REPLACE_STRATEGY_DEF_
#define GENERAL_PEER_REPLACE_STRATEGY_DEF_

#include <include/common.h>

namespace btorrent {

class general_peer_replace_strategy_t: public peer_replace_strategy_t {
public:
    general_peer_replace_strategy_t(const peer_piece_bitmap_t &bitmap);

    /* get replacement recommendation from active to inactive */
    virtual void get_peer_replacements(
        std::vector<peer_replacement_t> &replacements);

    /* adding to active */
    virtual bool get_peer_to_add(std::string &peer_id);

    /* removing from active */
    virtual void get_peers_to_remove(std::vector<std::string> &peer_ids);

    virtual void on_active_peer_added(const peer_piece_bitmap_t &bitmap);
    virtual void on_inactive_peer_added(const peer_piece_bitmap_t &bitmap);

    virtual void on_active_peer_removed(const peer_piece_bitmap_t &bitmap);
    virtual void on_inactive_peer_removed(const peer_piece_bitmap_t &bitmap);

    virtual void on_piece_part_aborted(const piece_part_request_t &req) {}
    virtual void on_piece_part_requested(const piece_part_request_t &req) {}
    virtual void on_piece_part_downloaded(const piece_part_request_t &req) {}

    virtual void on_piece_validation_done(size_type piece_index);
    virtual void on_piece_validation_failed(size_type piece_index) {}

private:
    peer_piece_bitmap_t m_mybitmap;

    struct peer_state_t {
        peer_state_t() : m_piece_count(0), m_piece_count_notmine(0),
              m_pending_count(0), m_bitmap("", 0, 0)
        {}

        peer_state_t(const peer_piece_bitmap_t &bitmap)
            : m_piece_count(0), m_piece_count_notmine(0),
              m_pending_count(0), m_bitmap(bitmap)
        {}

        std::string peer_id() const {
            return m_bitmap.peer_id();
        }

        size_type m_piece_count;
        size_type m_piece_count_notmine;
        size_type m_pending_count;

        peer_piece_bitmap_t m_bitmap;
    };
    typedef peer_state_t *ppeer_state_t;
    typedef std::map<std::string, ppeer_state_t> peer_map_type_t;

    ppeer_state_t get_min_item_from_list(const peer_map_type_t &mp);
    ppeer_state_t get_max_item_from_list(const peer_map_type_t &mp);

    int m_peer_replacement_threshold;
    std::map<std::string, peer_state_t> m_all_peers;

    peer_map_type_t m_active_peers;
    peer_map_type_t m_inactive_peers;

    typedef std::set<ppeer_state_t> peer_list_per_piece_t;
    std::vector<peer_list_per_piece_t> m_peer_per_piece_list;
};

}

#endif
