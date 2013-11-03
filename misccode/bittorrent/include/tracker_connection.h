#ifndef TRACKER_CONNECTION_DEF_
#define TRACKER_CONNECTION_DEF_

#include <include/common.h>

namespace btorrent {

class tracker_connection_t {
public:

    torrent &get_torrent() { return m_torrent; }

private:
    torrent &m_torrent;
};

}

#endif /* TRACKER_CONNECTION_DEF_ */
