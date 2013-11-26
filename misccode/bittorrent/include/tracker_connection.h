#ifndef TRACKER_CONNECTION_DEF_
#define TRACKER_CONNECTION_DEF_

#include <include/common.h>
#include <include/torrent.h>

namespace btorrent {

class tracker_connection_t {
public:
    tracker_connection_t(torrent_t &torrent)
        : m_torrent(torrent)
    {}

    virtual void start() = 0;
    virtual void finish() = 0;

    torrent_t &get_torrent() { return m_torrent; }
    const torrent_t &get_torrent() const { return m_torrent; }

private:
    torrent_t &m_torrent;
};

class tracker_connection_factory_t {
public:
    static tracker_connection_t *construct(torrent_t &torrent, const url_t &url);
};

}

#endif /* TRACKER_CONNECTION_DEF_ */
