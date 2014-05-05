#include <iostream>

#include <include/common.h>
#include <include/utils.h>
#include <include/torrent.h>

#include <include/tracker_connection.h>

#include <boost/smart_ptr.hpp>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: <path to torrent-file>" << std::endl;
        return 1;
    }

    btorrent::glog()->set_level(btorrent::logger_t::kdebug);

    /*
    boost::asio::io_service service;
    btorrent::torrent_info_t t = btorrent::construct_torrent_info(argv[1]);
    btorrent::torrent_t torrent(service, t);

    btorrent::url_t url(t.m_announce_url);
    boost::shared_ptr<btorrent::tracker_connection_t> conn(
        btorrent::tracker_connection_factory_t::construct(torrent, url));

    conn->start();
    service.run();
    */


    boost::asio::io_service service;
    btorrent::torrent_info_t t = btorrent::construct_torrent_info(argv[1]);
    btorrent::torrent_t torrent(service, t);
    torrent.start();
    service.run();

    return 0;
}
