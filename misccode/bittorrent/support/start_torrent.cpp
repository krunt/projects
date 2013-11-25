#include <iostream>

#include <include/common.h>
#include <include/utils.h>
#include <include/torrent.h>
#include <include/udp_tracker_connection.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: <path to torrent-file>" << std::endl;
        return 1;
    }

    boost::asio::io_service service;
    btorrent::torrent_info_t t = btorrent::construct_torrent_info(argv[1]);
    btorrent::torrent_t torrent(service, t);

    btorrent::url_t url(t.m_announce_url);
    btorrent::udp_tracker_connection_t conn(torrent);
    conn.start();
    service.run();

    return 0;
}
