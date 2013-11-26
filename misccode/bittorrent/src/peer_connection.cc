
#include <include/http_tracker_connection.h>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

namespace ip = boost::asio::ip;

namespace btorrent {

peer_connection_t::peer_connection_t(torrent_t &torrent,
        const std::string &host, int port)
    : m_torrent(torrent), m_host(host), m_port(port),
      m_resolver(get_torrent().io_service()),
      m_socket(get_torrent().io_service()),
      m_state(k_none)
{}

DEFINE_METHOD(void, peer_connection_t::start)
    m_state = k_connecting;

    m_data.resize(k_max_buffer_size);
    ip::tcp::resolver::query query(m_host, 
            boost::lexical_cast<std::string>(m_port));

    GLOG->debug("trying to connect to %s:%d", 
            m_host.c_str(), m_port);

    m_resolver.async_resolve(query,
        boost::bind(&peer_connection_t::on_resolve, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));
END_METHOD


DEFINE_METHOD(void, peer_connection_t::finish)
    if (m_state >= k_connected) {
        m_socket.close();
    }
    m_state = k_none;
END_METHOD


DEFINE_METHOD(void, peer_connection_t::on_resolve,
    const boost::system::error_code& err,
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    if (err) {
        GLOG->error("got error: ");
        finish();
        return;
    }

    GLOG->debug("resolved successfully to %s:%d", (*endpoint_iterator)
        .endpoint().address().to_string().c_str(), m_port);

    m_socket.async_connect(*endpoint_iterator,
        boost::bind(&peer_connection_t::on_connect, this,
            boost::asio::placeholders::error));
END_METHOD


DEFINE_METHOD(void, peer_connection_t::on_connect, 
        const boost::system::error_code& err) 

    if (err) {
        GLOG->error("got error: ");
        finish();
        return;
    }

    m_state = k_connected;

    return perform_handshake();     

END_METHOD


DEFINE_METHOD(void, peer_connection_t::perform_handshake)

    u8 handshake_request[1 + 19 + 8 + 20 + 20];
    u8 *p = handshake_request;

    *p++ = '\x13';
    memcpy(p, "BitTorrent protocol", 19); p += 19;
    memset(p, 0, 8); p += 8;
    memcpy(p, get_torrent().info_hash().get_digest().c_str(), 20); p += 20;
    memcpy(p, get_torrent().peer_id().c_str(), 20); p += 20;

    if (m_socket.send(boost::asio::buffer(handshake_request, 
        sizeof(handshake_request))) != sizeof(handshake_request))
    {
        GLOG->error("send handshake error");
        finish();
        return;
    }

    GLOG->debug("sent handshake successfully");      

    m_socket.async_receive(boost::asio::buffer(m_receive_buffer, k_max_buffer_size),
        boost::bind(&peer_connection_t::on_handshake_response, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));

END_METHOD


DEFINE_METHOD(void, peer_connection_t::on_handshake_response,
        const boost::system::error_code& err, size_t bytes_transferred)

    if (err || bytes_transferred < 1+19+8+20+20) { 
        GLOG->error("got error: ");
        finish();
        return; 
    }

    u8 *p = &m_receive_buffer[0];
    if (*p != '\x13' 
        || memcmp(p + 1, "BitTorrent protocol", 19)
            /* 8 zero bytes */
        || memcmp(p + 28, get_torrent().info_hash().get_digest().c_str(), 20)
        || memcmp(p + 48, get_torrent().info_hash().get_digest().c_str(), 20))
    {
        GLOG->error("mismatch in handshake response");
        finish();
        return; 
    }

    GLOG->debug("handshaked successfully");

    m_state = k_active;

END_METHOD

void peer_connection_t::send_keepalive() { send_common(t_keepalive, "");  }
void peer_connection_t::send_choke() { send_common(t_choke, ""); }
void peer_connection_t::send_unchoke() { send_common(t_unchoke, ""); }
void peer_connection_t::send_interested() { send_common(t_interested, ""); }
void peer_connection_t::send_notinterested() { send_common(t_not_interested, ""); }

void peer_connection_t::send_have(u32 index) { 
    u8 buffer[4]; u8 *p = buffer;
    int4store(p, index); 
    send_common(t_have, std::string(p, p + 4)); 
}

void peer_connection_t::send_bitfield(torrent_t::bitfield_type &t) { 
    send_common(t_bitfield, std::string(t.data(), t.size())); 
}

void peer_connection_t::send_request(u32 index, u32 begin, u32 length) { 
    u8 buffer[4 + 4 + 4]; u8 *p = buffer;
    int4store(p, index); p += 4;
    int4store(p, begin); p += 4;
    int4store(p, length); p += 4;
    send_common(t_request, std::string(p, p + 12)); 
}

void peer_connection_t::send_piece(u32 index, u32 begin, u32 piece) { 
    u8 buffer[4 + 4 + 4]; u8 *p = buffer;
    int4store(p, index); p += 4;
    int4store(p, begin); p += 4;
    int4store(p, piece); p += 4;
    send_common(t_piece, std::string(p, p + 12)); 
}

void peer_connection_t::send_request(u32 index, u32 begin, u32 length) { 
    u8 buffer[4 + 4 + 4]; u8 *p = buffer;
    int4store(p, index); p += 4;
    int4store(p, begin); p += 4;
    int4store(p, length); p += 4;
    send_common(t_cancel, std::string(p, p + 12)); 
}

DEFINE_METHOD(void, peer_connection_t::send_common, message_type_t type, 
        const std::string &payload)

    u8 packet_prefix[1+4]; u8 *p = packet_prefix;

    if (type == t_keepalive) {
        int4store(p, 0); p += 4;
    } else {
        *p++ = static_cast<u8>(type);
        int4store(p, static_cast<u32>(payload.size())); p += 4;
    }

    if ((m_socket.send(boost::asio::buffer(packet_prefix, 
                    p - packet_prefix)) != p - packet_prefix)
        || m_socket.send(boost::asio::buffer(payload)) != payload.size())
    {
        GLOG->error("send %d error", type);
        finish();
        return;
    }

    GLOG->debug("sent %d packet successfully", type);

    /* this type expects payload-response */
    if (type == t_request) {
        m_receive_buffer.resize(k_max_buffer_size);
        m_socket.async_receive(
            boost::asio::buffer(m_receive_buffer, k_max_buffer_size),
            boost::bind(&peer_connection_t::on_data_received, 
                this, boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

END_METHOD

DEFINE_METHOD(void, peer_connection_t::on_data_received,
    const boost::system::error_code& err, size_t bytes_transferred)

    if (err == boost::asio::error::operation_aborted) {
        return;
    }

    if (err) {
        GLOG->debug("got error");
        finish();
        return;
    }

    if (bytes_transferred) {
        GLOG->debug("got %d bytes", bytes_transferred);

        m_receive_buffer.resize(bytes_transferred);
        utils::read_available_from_socket(m_socket, m_receive_buffer);
        m_input_message_stream.feed_data(m_receive_buffer);

        /* we handled all data already */
        m_socket.cancel();

        m_receive_buffer.resize(k_max_buffer_size);
        m_socket.async_receive(
            boost::asio::buffer(m_receive_buffer, k_max_buffer_size),
            boost::bind(&peer_connection_t::on_data_received, 
                this, boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    if (m_input_message_stream.has_pending()) {

    }

END_METHOD

message_stream_t::feed_data() {

}


}
