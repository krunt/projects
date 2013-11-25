#include <include/torrent.h>

namespace btorrent {

void torrent_t::add_peer(const boost::asio::ip::address &host, int port) {
    glog()->info(host.to_string()); 
}

void torrent_t::get_announce_urls(std::vector<url_t> &urls) {
    urls.push_back(url_t(m_torrent_info.m_announce_url));
    for (int i = 0; i < m_torrent_info.m_announce_list.size(); ++i) {
        urls.push_back(url_t(m_torrent_info.m_announce_list[i]));
    }
}

}
