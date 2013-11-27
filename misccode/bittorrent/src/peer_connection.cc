
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

    setup_receive_callback();

END_METHOD

void peer_connection_t::send_keepalive() { send_common(t_keepalive, "");  }
void peer_connection_t::send_choke() { 
    send_common(t_choke, ""); 
    m_connection_state.he_choked = true;
}
void peer_connection_t::send_unchoke() { 
    send_common(t_unchoke, ""); 
    m_connection_state.he_choked = false;
}
void peer_connection_t::send_interested() { 
    send_common(t_interested, ""); 
    m_connection_state.me_interested = true;
}
void peer_connection_t::send_notinterested() { 
    send_common(t_not_interested, ""); 
    m_connection_state.me_interested = false;
}
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

    }

    if (m_input_message_stream.has_pending()) {
        process_input_messages();
    }

    setup_receive_callback();

END_METHOD


void peer_connection_t::process_input_messages() {
    const std::vector<message_t> &messages = m_input_message_stream.get_pending();
    for (int i = 0; i < messages.size(); ++i) {
        switch (messages[i].type) {
        case t_keepalive: on_keepalive(messages[i]); break;
        case t_choke: on_choke(messages[i]); break;
        case t_unchoke: on_unchoke(messages[i]); break;
        case t_interested: on_interested(messages[i]); break;
        case t_not_interested: on_notinterested(messages[i]); break;
        case t_have: on_have(messages[i]); break;
        case t_bitfield: on_bitfield(messages[i]); break;
        case t_request: /* ignore */ break;
        case t_piece: on_piece(messages[i]); break;
        case t_cancel: on_cancel(messages[i]); break;
        };
    }
    m_input_message_stream.clear_pending();
}


void peer_connection_t::on_keepalive(const message_t &message) {}
void peer_connection_t::on_choke(const message_t &message) {
    m_connection_state.me_choked = true;
}
void peer_connection_t::on_unchoke(const message_t &message) {
    m_connection_state.me_choked = false;
}
void peer_connection_t::on_interested(const message_t &message) {
    m_connection_state.he_interested = true;
}
void peer_connection_t::on_notinterested(const message_t &message) {
    m_connection_state.he_interested = false;
}
void peer_connection_t::on_have(const message_t &message) {}
void peer_connection_t::on_bitfield(const message_t &message) {}
void peer_connection_t::on_piece(const message_t &message) {}
void peer_connection_t::on_cancel(const message_t &message) {}

void peer_connection_t::setup_receive_callback() {
    m_receive_buffer.resize(k_max_buffer_size);
    m_socket.async_receive(
        boost::asio::buffer(m_receive_buffer, k_max_buffer_size),
        boost::bind(&peer_connection_t::on_data_received, 
            this, boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}


void peer_connection_t::message_stream_t::feed_data(
    const std::vector<u8> &str) 
{
    if (str.empty())
        return;

    size_type pending_bytes = str.size();
    u8 *p = &str[0];

    while (pending_bytes) {

    switch (m_state) {
    case s_length: {
        if (m_pending_length_count + pending_bytes >= 4) {
            memcpy(m_current.lenpart.buf + m_pending_length_count, 
                p, 4 - m_pending_length_count);
            p += 4 - m_pending_length_count;
            pending_bytes -= 4 - m_pending_length_count;
            m_pending_length_count = 4;
            int4get(m_current.lenpath.length, m_current.lenpath.buf);

            /* if is keepalive */
            if (m_current.lenpath.length == 0) {
                m_current.type = peer_connection_t::t_keepalive;
                m_pending_messages.push_back(m_current);
                reset();
            } else {
                m_state = s_type;
            }

        } else {
            memcpy(m_current.lenpart.buf + m_pending_length_count, 
                p, pending_bytes);
            p += pending_bytes;
            m_pending_length_count += pending_bytes;
            pending_bytes = 0;
        }
        break;
    }

    case s_type: {
        m_state = static_cast<state_t>(*p++); 
        --pending_bytes;
        break;
    }

    case s_payload: {
        if (m_current.payload.size() + pending_bytes >= m_current.lenpart.length) {
            size_type to_copy = m_current.lenpart.length - m_current.payload.size();
            payload.resize(payload.size() + to_copy);
            memcpy(&payload[payload.size() - to_copy], p, to_copy);
            p += to_copy;
            pending_bytes -= to_copy;

            /* adding message to pending */
            m_pending_messages.push_back(m_current);
            reset();

        } else {
            payload.resize(payload.size() + pending_bytes);
            memcpy(&payload[payload.size() - pending_bytes], p, pending_bytes);
            p += pending_bytes;
            pending_bytes = 0;
        }
        break; 
    }};

    }
}


}
