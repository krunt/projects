#include <include/torrent.h>

namespace btorrent {

torrent_t::torrent_t(boost::asio::io_service &io_service_arg, 
        const torrent_info_t &info)
    : m_io_service(io_service_arg), m_bytes_downloaded(0), m_bytes_left(0),
    m_bytes_uploaded(0), m_torrent_info(info)
{
    /* computing unique peer_id */
    char c[20];
    btorrent::generate_random(c, sizeof(c));
    m_peer_id = std::string(c, c + 20);

    /* computing total file size */
    for (int i = 0; i < m_torrent_info.m_files.size(); ++i) {
        m_bytes_left += m_torrent_info.m_files[i].m_size;
    }
}

DEFINE_METHOD(void, torrent_t::add_peer, const std::string &host, int port) 
    GLOG->info("got %s:%d", host.c_str(), port); 
END_METHOD

void torrent_t::get_announce_urls(std::vector<url_t> &urls) {
    urls.push_back(url_t(m_torrent_info.m_announce_url));
    for (int i = 0; i < m_torrent_info.m_announce_list.size(); ++i) {
        urls.push_back(url_t(m_torrent_info.m_announce_list[i]));
    }
}

}
