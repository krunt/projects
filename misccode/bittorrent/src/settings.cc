#include <include/settings.h>

namespace btorrent {

settings_t global_settings;
settings_t *gsettings() { return &global_settings; }

settings_t::settings_t()
        : m_piece_part_size(16 << 10), /* 16 kb */
          m_torrents_data_path("/home/akuts/datastorage"),
          m_max_check_connections(3),
          m_max_active_connections(10),
          m_download_time_interval(1),
          m_max_pending_requests_per_peer(20),
          m_peer_replacement_threshold(10),
          m_piece_pick_strategy_type(piece_pick_strategy::most_rarest),
          m_peer_replace_strategy_type(peer_replace_strategy::general)
{}

}

