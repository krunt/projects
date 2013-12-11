#include <include/common.h>

#include <include/http_tracker_connection.h>
#include <include/udp_tracker_connection.h>

namespace btorrent {

tracker_connection_t *tracker_connection_factory_t::construct(
        torrent_t &torrent, const url_t &url) 
{
    if (url.get_scheme() == "http") {
        return new http_tracker_connection_t(torrent, url);
    }

    if (url.get_scheme() == "udp") {
        return new udp_tracker_connection_t(torrent, url);
    }
    
    throw std::runtime_error("unknown tracker_connection scheme");
}

}
