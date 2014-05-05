#ifndef SETTINGS_DEF_
#define SETTINGS_DEF_

#include <include/common.h>
#include <include/piece_pick_strategy.h>
#include <include/peer_replace_strategy.h>

namespace btorrent {

struct settings_t: public boost::noncopyable {
    settings_t();

    int m_piece_part_size;
    std::string m_torrents_data_path;
    int m_max_check_connections;
    int m_max_active_connections;
    int m_download_time_interval;
    int m_max_pending_requests_per_peer;
    int m_peer_replacement_threshold;
    piece_pick_strategy::k_piece_pick_strategy_type m_piece_pick_strategy_type;
    peer_replace_strategy::k_peer_replace_strategy_type m_peer_replace_strategy_type;
};

extern settings_t *gsettings();

}

#endif /* SETTINGS_DEF_ */
