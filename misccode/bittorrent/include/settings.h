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
    {}

    int m_piece_part_size;
    std::string m_torrents_data_path;
    int m_max_check_connections;
    int m_max_active_connections;
};

extern settings_t *gsettings();

}

#endif /* SETTINGS_DEF_ */
