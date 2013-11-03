#ifndef UDP_TRACKER_CONNECTION_DEF_
#define UDP_TRACKER_CONNECTION_DEF_

#include <include/common.h>
#include <boost/asio.hpp>

namespace btorrent {

class udp_tracker_connection_t: public tracker_connection_t {
public:
    udp_tracker_connection_t(torrent &torrent, 
            const std::string &host, int port)
        : m_torrent(torrent),
          m_host(host), m_port(port),
          m_resolver(m_torrent.io_service())
    {}

private:
    std::string m_host;
    int m_port;

    namespace udp = boost::asio::ip::udp;
    udp::resolver m_resolver;
    udp::socket m_socket;

    u32 m_transaction_id;
    u64 m_connection_id;

    static const int k_max_buffer_size = 4096;
    boost::array<char, k_max_buffer_size> m_data;

    enum action_t { k_connect = 0, k_announce = 1, };
};

}


#endif /* UDP_TRACKER_CONNECTION_DEF_ */
