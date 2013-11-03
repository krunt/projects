
#include <include/udp_tracker_connection_t.h>

namespace ip = boost::asio::ip;

namespace btorrent {

void udp_tracker_connection_t::start() {
    ip::ucp::resolver query(m_host, m_port);

    btorrent::generate_random(static_cast<char*>(&m_transaction_id), 
            sizeof(m_transaction_id));
    btorrent::generate_random(static_cast<char*>(&m_connection_id), 
            sizeof(m_connection_id));

    m_resolver.async_resolve(query,
        boost::bind(&udp_tracker_connection_t::on_resolve, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));
}

void udp_tracker_connection_t::on_resolve(
    const boost::system::error_code& err,
    tcp::resolver::iterator endpoint_iterator)
{
    if (!err) {
        boost::asio::async_connect(m_socket, endpoint_iterator,
            boost::bind(&udp_tracker_connection_t::on_connect,
                boost::asio::placeholders::error));
    }
}

void udp_tracker_connection_t::on_connect(const boost::system::error_code& err) {
    if (err) { return; }

    u8 connection_request[8 + 4 + 4], *p;
    p = connection_request;

    int8store(p, m_connection_id); p += 8;
    int4store(p, k_connect); p += 4;
    int4store(p, m_transaction_id); p += 4;

    m_socket.async_receive(m_data, boost::asio::buffer(m_data, k_max_buffer_size),
            boost::bind(&udp_tracker_connection_t::on_received_connect_request, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
}

void udp_tracker_connection_t::on_received_connect_request(
        size_t bytes_transferred) {

    if (err || bytes_transferred < 16) { return; }

    u32 response_action, response_transaction_id, response_connection_id;
    u8 *p = m_data.begin();

    int4get(response_action, p); p += 4;
    int4get(response_transaction_id, p); p += 4;
    int8get(response_connection_id, p); p += 8;

    if (response_transaction_id != m_transaction_id
            || response_action != k_connect) 
    {
        return;
    }

    m_connection_id = response_connection_id;

    /* sending announce-request */
    u8 announce_request[98], *p;

    int8store(p, m_connection_id); p += 8;
    int4store(p, k_announce); p += 4;
    int4store(p, m_transaction_id); p += 4;
    memcpy(p, get_torrent().info_hash().c_str(), 20); p += 20;
    memcpy(p, get_torrent().peerid().c_str(), 20); p += 20;
    int8store(p, get_torrent().bytes_downloaded()); p += 8;
    int8store(p, get_torrent().bytes_left()); p += 8;
    int8store(p, get_torrent().bytes_uploaded()); p += 8;
    int4store(p, 0); p += 4; /* TODO event */
    int4store(p, 0); p += 4; /* ip-address */
    int4store(p, 0); p += 4; /* key */
    int4store(p, -1); p += 4; /* num_want */
    int2store(p, 0); p += 2; /* port */

    m_socket.async_receive(m_data, boost::asio::buffer(m_data, k_max_buffer_size),
            boost::bind(&udp_tracker_connection_t::on_received_connect_request, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
}

}
