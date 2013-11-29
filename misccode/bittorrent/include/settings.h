#ifndef SETTINGS_DEF_
#define SETTINGS_DEF_

#include <include/common.h>

namespace btorrent {

struct settings_t: public boost::noncopyable {
    settings_t()
        : m_piece_part_size(16 << 10), /* 16 kb */
          m_torrents_data_path("/home/akuts/datastorage"),
          m_max_check_connections(3),
          m_max_active_connections(10),
          m_download_time_interval(1),
          m_max_pending_requests_per_peer(3),
          m_peer_replacement_threshold(10),
          m_piece_pick_strategy_type(piece_pick_strategy::most_rarest),
          m_peer_replace_strategy_type(peer_replace_strategy::general)
    {}

    int m_piece_part_size;
    std::string m_torrents_data_path;
    int m_max_check_connections;
    int m_max_active_connections;
    int m_download_time_interval;
    int m_max_pending_requests_per_peer;
    int m_peer_replacement_threshold;
    k_piece_pick_strategy_type m_piece_pick_strategy_type;
    k_peer_replace_strategy_type m_peer_replace_strategy_type;
};

extern settings_t *gsettings();

}

#endif /* SETTINGS_DEF_ */
