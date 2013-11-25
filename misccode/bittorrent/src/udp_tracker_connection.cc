
#include <include/udp_tracker_connection.h>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/bind.hpp>

namespace ip = boost::asio::ip;

namespace btorrent {

udp_tracker_connection_t::udp_tracker_connection_t(torrent_t &torrent)
    : m_torrent(torrent),
      m_resolver(m_torrent.io_service()),
      m_socket(m_torrent.io_service()),
      m_state(s_none),
      m_timeout_timer(m_torrent.io_service())
{
    std::vector<url_t> announce_urls;
    m_torrent.get_announce_urls(announce_urls);
    for (int i = 0; i < announce_urls.size(); ++i) {
        if (announce_urls[i].get_scheme() == "udp") {
            m_announce_urls.push_back(announce_urls[i]);
        }
    }
    m_current_announce_url_index = 0;
}

void udp_tracker_connection_t::start() {
    m_state = s_announcing;

    m_host = m_announce_urls[m_current_announce_url_index].get_host();
    m_port = m_announce_urls[m_current_announce_url_index].get_port();

    ip::udp::resolver::query query(m_host, 
            boost::lexical_cast<std::string>(m_port));

    glog()->debug("udp-start trying to connect to %s:%d", 
            m_host.c_str(), m_port);
    m_resolver.async_resolve(query,
        boost::bind(&udp_tracker_connection_t::on_resolve, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));
}

void udp_tracker_connection_t::finish(bool success) {
    glog()->debug("udp-finish to %s/%d", m_host.c_str(), m_port);

    m_state = s_none;
    m_timeout_step = 0;
    cancel_timeout();

    if (!success) {
        (++m_current_announce_url_index) %= m_announce_urls.size();
        start();
    } else {
        m_current_announce_url_index = 0;
    }
}

void udp_tracker_connection_t::on_timer_timeout() {
    if (++m_timeout_step > 8) {
        glog()->error("udp_tracker_connection_t request timeout out");
        finish();
        return;
    }

    glog()->debug("got timeout, configured timer for %d seconds", 
            timeout_in_seconds());

    if (m_socket.send(boost::asio::buffer(static_cast<u8*>(&*(m_send_buffer.begin())),
            m_send_buffer.size())) != m_send_buffer.size())
    {
        glog()->error("sending %d bytes by timeout error", m_send_buffer.size());
        finish();
        return;
    }

    m_timeout_timer.expires_from_now(boost::posix_time::seconds(
                timeout_in_seconds()));
    m_timeout_timer.async_wait(boost::bind(
        &udp_tracker_connection_t::on_timer_timeout, this));
}

void udp_tracker_connection_t::configure_timeout() {
    m_timeout_step = 0;
    m_timeout_timer.expires_from_now(boost::posix_time::seconds(
                timeout_in_seconds()));
    m_timeout_timer.async_wait(boost::bind(
        &udp_tracker_connection_t::on_timer_timeout, this));

    glog()->debug("configured timer for %d seconds", timeout_in_seconds());
}

void udp_tracker_connection_t::cancel_timeout() {
    m_timeout_timer.cancel();
}

void udp_tracker_connection_t::on_resolve(
    const boost::system::error_code& err,
    boost::asio::ip::udp::resolver::iterator endpoint_iterator)
{
    if (err) {
        glog()->error("udp_tracker_connection_t::on_resolve error");
        finish();
        return;
    }

    glog()->debug("resolve successfully to %s:%d", (*endpoint_iterator)
        .endpoint().address().to_string().c_str(), m_port);

    m_socket.async_connect(*endpoint_iterator,
        boost::bind(&udp_tracker_connection_t::on_connect, this,
            boost::asio::placeholders::error));
}

void udp_tracker_connection_t::on_connect(const boost::system::error_code& err) {
    if (err) {
        glog()->error("udp_tracker_connection_t::on_connect error");
        finish();
        return;
    }

    btorrent::generate_random(reinterpret_cast<char*>(&m_transaction_id), 
            sizeof(m_transaction_id));
    btorrent::generate_random(reinterpret_cast<char*>(&m_connection_id), 
            sizeof(m_connection_id));

    u8 connection_request[8 + 4 + 4], *p;
    p = connection_request;

    int8store(p, m_connection_id); p += 8;
    int4store(p, k_connect); p += 4;
    int4store(p, m_transaction_id); p += 4;

    glog()->debug("sending connection_request");

    m_send_buffer.assign(connection_request, 
            connection_request + sizeof(connection_request));
    if (m_socket.send(boost::asio::buffer(connection_request, 
        sizeof(connection_request))) != sizeof(connection_request)) 
    {
        glog()->error("udp_tracker_connection_t::send connection_request error");
        finish();
        return;
    }

    glog()->debug("sending connection_request done.");

    configure_timeout();
    m_socket.async_receive(boost::asio::buffer(m_data, k_max_buffer_size),
        boost::bind(&udp_tracker_connection_t::on_received_connect_response, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
}

void udp_tracker_connection_t::on_received_connect_response(
        const boost::system::error_code& err, size_t bytes_transferred) 
{
    if (err || bytes_transferred < 16) { 
        glog()->error(
    "udp_tracker_connection_t::on_received_connect_response error");
        finish();
        return; 
    }

    glog()->debug("on_received_connect_response got");

    u32 response_action, response_transaction_id, response_connection_id;
    u8 *p = reinterpret_cast<u8*>(m_data.begin());

    int4get(response_action, p); p += 4;
    int4get(response_transaction_id, p); p += 4;
    int8get(response_connection_id, p); p += 8;

    if (response_transaction_id != m_transaction_id
            || response_action != k_connect) 
    {
        glog()->error(
    "udp_tracker_connection_t::on_received_connect_response mismatch response");
        finish();
        return;
    }

    m_connection_id = response_connection_id;

    /* sending announce-request */
    u8 announce_request[98];

    p = announce_request;
    int8store(p, m_connection_id); p += 8;
    int4store(p, k_announce); p += 4;
    int4store(p, m_transaction_id); p += 4;
    memcpy(p, get_torrent().info_hash().get_digest().c_str(), 20); p += 20;
    memcpy(p, get_torrent().peer_id().c_str(), 20); p += 20;
    int8store(p, get_torrent().bytes_downloaded()); p += 8;
    int8store(p, get_torrent().bytes_left()); p += 8;
    int8store(p, get_torrent().bytes_uploaded()); p += 8;
    int4store(p, 0); p += 4; /* TODO event */
    int4store(p, 0); p += 4; /* ip-address */
    int4store(p, 0); p += 4; /* key */
    int4store(p, -1); p += 4; /* num_want */
    int2store(p, 0); p += 2; /* port */

    m_send_buffer.assign(announce_request, 
            announce_request + sizeof(announce_request));
    if (m_socket.send(boost::asio::buffer(announce_request, 
        sizeof(announce_request))) != sizeof(announce_request)) 
    {
        glog()->error("udp_tracker_connection_t::send announce_request error");
        finish();
        return;
    }

    configure_timeout();
    m_socket.async_receive(boost::asio::buffer(m_data, k_max_buffer_size),
        boost::bind(&udp_tracker_connection_t::on_received_announce_response, this,
        boost::asio::placeholders::error, 
        boost::asio::placeholders::bytes_transferred));
}

void udp_tracker_connection_t::on_received_announce_response(
        const boost::system::error_code& err, size_t bytes_transferred) 
{
    if (err || bytes_transferred < 20) { 
        glog()->error(
                "udp_tracker_connection_t::on_received_announce_response error");
        finish();
        return; 
    }

    u32 action, transaction_id, interval, leechers, seeders, ip_address, ip_port;
    u8 *p = reinterpret_cast<u8*>(m_data.begin());

    int4get(action, p); p += 4;
    int4get(transaction_id, p); p += 4;
    int8get(interval, p); p += 4;
    int8get(leechers, p); p += 4;
    int8get(seeders, p); p += 4;

    bytes_transferred -= 20;
    while (bytes_transferred >= 6) {
        int4get(ip_address, p); p += 4;
        int2get(ip_port, p); p += 2;
        get_torrent().add_peer(
            boost::asio::ip::address(boost::asio::ip::address_v4(ip_address)), 
                ip_port);
    }

    /* TODO read more hosts here */

    finish(true);
}

}
