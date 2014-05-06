
#include <include/udp_tracker_connection.h>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/bind.hpp>

namespace ip = boost::asio::ip;

namespace btorrent {

udp_tracker_connection_t::udp_tracker_connection_t(torrent_t &torrent,
       const url_t &announce_url)
    : tracker_connection_t(torrent),
      m_host(announce_url.get_host()), m_port(announce_url.get_port()),
      m_resolver(get_torrent().io_service()),
      m_socket(get_torrent().io_service()),
      m_state(s_none),
      m_timeout_timer(get_torrent().io_service())
{}

void udp_tracker_connection_t::start() {
    m_state = s_announcing;
    m_data.resize(k_max_buffer_size);

    ip::udp::resolver::query query(m_host, 
            boost::lexical_cast<std::string>(m_port));

    glog()->debug("udp-start trying to connect to %s:%d", 
            m_host.c_str(), m_port);

    m_resolver.async_resolve(query,
        boost::bind(&udp_tracker_connection_t::on_resolve, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));
}

void udp_tracker_connection_t::finish() {
    glog()->debug("udp-finish to %s/%d", m_host.c_str(), m_port);

    m_state = s_none;
    m_timeout_step = 0;
    cancel_timeout();
}

void udp_tracker_connection_t::on_timer_timeout(
        const boost::system::error_code& err) 
{
    if (err == boost::asio::error::operation_aborted) {
        return;
    }

    if (++m_timeout_step > 8) {
        glog()->error("udp_tracker_connection_t request timeout out");
        finish();
        return;
    }

    glog()->debug("got timeout, configured timer for %d seconds", 
            timeout_in_seconds());

    if (m_socket.send(boost::asio::buffer(
                    static_cast<u8*>(&*(m_send_buffer.begin())),
            m_send_buffer.size())) != m_send_buffer.size())
    {
        glog()->error("sending %d bytes by timeout error", m_send_buffer.size());
        finish();
        return;
    }

    m_timeout_timer.expires_from_now(boost::posix_time::seconds(
                timeout_in_seconds()));
    m_timeout_timer.async_wait(boost::bind(
        &udp_tracker_connection_t::on_timer_timeout, this,
        boost::asio::placeholders::error));
}

void udp_tracker_connection_t::configure_timeout() {
    m_timeout_step = 0;
    m_timeout_timer.expires_from_now(boost::posix_time::seconds(
                timeout_in_seconds()));
    m_timeout_timer.async_wait(boost::bind(
        &udp_tracker_connection_t::on_timer_timeout, this,
        boost::asio::placeholders::error));

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

    boost::asio::ip::udp::endpoint endpoint(*endpoint_iterator);
    m_socket.async_connect(endpoint,
        boost::bind(&udp_tracker_connection_t::on_connect, this,
            boost::asio::placeholders::error, ++endpoint_iterator));
}

DEFINE_METHOD(void, udp_tracker_connection_t::on_connect,
            const boost::system::error_code& err,
    boost::asio::ip::udp::resolver::iterator endpoint_iterator)

    if (err) {
        if (endpoint_iterator == boost::asio::ip::udp::resolver::iterator()) {
            GLOG->error("got error: " + err.message());
            finish();
            return;
        }

        m_socket.close();
        boost::asio::ip::udp::endpoint endpoint(*endpoint_iterator);
        m_socket.async_connect(endpoint,
            boost::bind(&udp_tracker_connection_t::on_connect, this,
                boost::asio::placeholders::error, ++endpoint_iterator));
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
                boost::asio::placeholders::bytes_transferred,
                endpoint_iterator));
END_METHOD


DEFINE_METHOD(void, udp_tracker_connection_t::on_received_connect_response,
        const boost::system::error_code& err, size_t bytes_transferred,
        boost::asio::ip::udp::resolver::iterator endpoint_iterator) 

    if (err) {
        if (endpoint_iterator == boost::asio::ip::udp::resolver::iterator()) {
            GLOG->error("got error: " + err.message());
            finish();
            return;
        }

        cancel_timeout();
        m_socket.close();
        boost::asio::ip::udp::endpoint endpoint(*endpoint_iterator);
        m_socket.async_connect(endpoint,
            boost::bind(&udp_tracker_connection_t::on_connect, this,
                boost::asio::placeholders::error, ++endpoint_iterator));
        return; 
    }

    if (bytes_transferred < 16) {
        GLOG->error("not enough bytes received = %d", bytes_transferred);
        finish();
        return; 
    }

    GLOG->debug("on_received_connect_response got");

    u32 response_action, response_transaction_id, response_connection_id;
    u8 *p = reinterpret_cast<u8*>(m_data.data());

    int4get(response_action, p); p += 4;
    int4get(response_transaction_id, p); p += 4;
    int8get(response_connection_id, p); p += 8;

    if (response_transaction_id != m_transaction_id
            || response_action != k_connect) 
    {
        GLOG->error("mismatch response");
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

END_METHOD

DEFINE_METHOD(void, udp_tracker_connection_t::on_received_announce_response,
        const boost::system::error_code& err, size_t bytes_transferred) 

    if (err) { 
        GLOG->error("error=" + err.message());
        finish();
        return; 
    }

    if (bytes_transferred < 20) { 
        GLOG->error("not enough bytes received %d", bytes_transferred);
        finish();
        return; 
    }

    m_data.resize(bytes_transferred);
    utils::read_available_from_socket(m_socket, m_data);
    bytes_transferred = m_data.size();

    u16 ip_port;
    u32 action, transaction_id, interval, leechers, seeders, ip_address; 

    u8 *p = reinterpret_cast<u8*>(&m_data[0]);

    int4get(action, p); p += 4;
    int4get(transaction_id, p); p += 4;
    int8get(interval, p); p += 4;
    int8get(leechers, p); p += 4;
    int8get(seeders, p); p += 4;

    bytes_transferred -= 20;
    while (bytes_transferred >= 6) {
        int4get(ip_address, p); p += 4;
        int2get(ip_port, p); p += 2;
        get_torrent().add_peer(utils::ipv4_to_string(ip_address), 
                utils::ipv4_to_string(ip_address), ip_port);
        bytes_transferred -= 6;
    }
    finish();
END_METHOD

}
