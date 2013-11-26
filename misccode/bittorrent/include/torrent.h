#ifndef TORRENT_DEF_
#define TORRENT_DEF_

#include <include/common.h>
#include <boost/asio.hpp>

namespace btorrent {

class torrent_t {
public:
    torrent_t(boost::asio::io_service &io_service_arg, const torrent_info_t &info);

    boost::asio::io_service &io_service() { return m_io_service; }

    void add_peer(const std::string &host, int port);
    void on_piece_block(
    
    void get_announce_urls(std::vector<url_t> &urls);
    const sha1_hash_t &info_hash() const { return m_torrent_info.m_info_hash; }
    const std::string &peer_id() const { return m_peer_id; }
    u64 bytes_downloaded() const { return m_bytes_downloaded; }
    u64 bytes_left() const { return m_bytes_left; }
    u64 bytes_uploaded() const { return m_bytes_uploaded; }

private:
    boost::asio::io_service &m_io_service;
    std::string m_peer_id;
    u64 m_bytes_downloaded;
    u64 m_bytes_left;
    u64 m_bytes_uploaded;
    torrent_info_t m_torrent_info;
};

}


#endif /* TORRENT_DEF_ */
