#include <include/common.h>
#include <include/torrent.h>
#include <include/peer_connection.h>

#include <include/http_tracker_connection.h>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

namespace ip = boost::asio::ip;

namespace btorrent {

DEFINE_METHOD(void, peer_connection_t::start)
    m_state = k_connecting;
    m_in_flight = false;

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
    m_resolver.cancel();
    m_socket.cancel();

    if (m_state >= k_connected) {
        m_socket.close();
    }

    if (m_connection_lost_callback) {
        m_connection_lost_callback();
    }

    m_state = k_none;
END_METHOD


DEFINE_METHOD(void, peer_connection_t::on_resolve,
    const boost::system::error_code& err,
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator)

    if (err == boost::asio::error::operation_aborted) {
        finish();
        return;
    }

    if (err) {
        GLOG->error("got error: " + err.message());
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

    if (err == boost::asio::error::operation_aborted) {
        finish();
        return;
    }

    if (err) {
        GLOG->error("got error: " + err.message());
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

    m_receive_buffer.resize(k_max_buffer_size);
    boost::asio::async_read(m_socket,
        boost::asio::buffer(m_receive_buffer, k_max_buffer_size),
        boost::asio::transfer_at_least(1),
        boost::bind(&peer_connection_t::on_handshake_response, this,
            boost::asio::placeholders::error, 
            boost::asio::placeholders::bytes_transferred));

END_METHOD


DEFINE_METHOD(void, peer_connection_t::on_handshake_response,
        const boost::system::error_code& err, size_t bytes_transferred)

    if (err == boost::asio::error::operation_aborted) {
        GLOG->debug("operation-aborted");
        finish();
        return;
    }

    if (err) { 
        GLOG->error("got error: " + err.message());
        finish();
        return;
    }

    if (!bytes_transferred) {
        GLOG->debug("unexpected connection-close");
        finish();
        return;
    }

    if (bytes_transferred < 1+19+8+20+20) {
        GLOG->debug("not enough bytes transferred %d bytes", bytes_transferred);
        finish();
        return;
    }

    u8 *p = &m_receive_buffer[0];
    if (*p != '\x13' 
        || memcmp(p + 1, "BitTorrent protocol", 19)
            /* 8 zero bytes */
        || memcmp(p + 28, get_torrent().info_hash().get_digest().c_str(), 20)
        || memcmp(p + 48, m_peer_id.data(), 20))
    {
        GLOG->error("mismatch in handshake response");
        GLOG->debug("handshake=" +
            hex_encode(std::string((char*)m_receive_buffer.data(), 68)));
        finish();
        return; 
    }

    GLOG->debug("handshaked successfully");

    if (m_handshake_done_callback) {
        m_handshake_done_callback();
    }

    m_state = k_active;

    setup_receive_callback();

END_METHOD

void peer_connection_t::send_keepalive(bool async) { 
    send_common(t_keepalive, "", async);
}
void peer_connection_t::send_choke(bool async) { 
    send_common(t_choke, "", async);
    m_connection_state.he_choked = true;
}
void peer_connection_t::send_unchoke(bool async) { 
    send_common(t_unchoke, "", async);
    m_connection_state.he_choked = false;
}
void peer_connection_t::send_interested(bool async) { 
    send_common(t_interested, "", async); 
    m_connection_state.me_interested = true;
}
void peer_connection_t::send_notinterested(bool async) { 
    send_common(t_not_interested, "", async); 
    m_connection_state.me_interested = false;
}
void peer_connection_t::send_have(u32 index, bool async) { 
    u8 buffer[4]; u8 *p = buffer;
    int4store(p, index); 
    send_common(t_have, std::string((char *)buffer, p - buffer), async); 
}

/*
void peer_connection_t::send_bitfield(torrent_t::bitfield_type &t) { 
    send_common(t_bitfield, std::string(t.data(), t.size())); 
}
*/

void peer_connection_t::send_request(u32 index, u32 begin, 
        u32 length, bool async) { 
    u8 buffer[4 + 4 + 4]; u8 *p = buffer;
    int4store(p, index); p += 4;
    int4store(p, begin); p += 4;
    int4store(p, length); p += 4;
    send_common(t_request, std::string((char *)buffer, p - buffer), async); 
}

void peer_connection_t::send_piece(u32 index, u32 begin, 
        const std::vector<u8> &data, bool async) 
{
    u8 buffer[4 + 4]; u8 *p = buffer;
    int4store(p, index); p += 4;
    int4store(p, begin); p += 4;
    send_common(t_piece, std::string((char *)buffer, p - buffer) 
        + std::string(reinterpret_cast<const char*>(&data[0]), data.size()),
        async);
}

DEFINE_METHOD(void, peer_connection_t::send_common, message_type_t type, 
        const std::string &payload, bool async)

    u8 packet_prefix[4+1]; u8 *p = packet_prefix;

    if (type == t_keepalive) {
        int4store(p, 0); p += 4;
    } else {
        int4store(p, static_cast<u32>(1 + payload.size())); p += 4;
        *p++ = static_cast<u8>(type);
    }

    if (async) {
        m_send_packets.push_back(packet_t());

        packet_t &packet = m_send_packets.back();

        packet.m_bufs.push_back(
            std::string((char*)packet_prefix, p - packet_prefix));

        if (!payload.empty()) {
            packet.m_bufs.push_back(payload);
        }

        GLOG->debug("packet was enqueued with %d bytes %s", 
            p - packet_prefix + payload.size(),
            hex_encode(std::string((char*)packet_prefix, 
                    p - packet_prefix)).c_str());

        m_send_packets.push_back(packet);
        send_output_queue();
        
    } else {
        if ((m_socket.send(boost::asio::buffer(packet_prefix, 
                        p - packet_prefix)) != p - packet_prefix)
            || (!payload.empty()
            && m_socket.send(boost::asio::buffer(payload)) != payload.size()))
        {
            GLOG->error("send %d error", type);
            finish();
            return;
        }
    
        GLOG->debug("sent type=%d packet successfully", type);
    }

END_METHOD


DEFINE_METHOD(void, peer_connection_t::on_data_received,
    const boost::system::error_code& err, size_t bytes_transferred)

    if (err == boost::asio::error::operation_aborted) {
        return;
    }

    if (err) {
        GLOG->debug("got error " + err.message());
        finish();
        return;
    }

    if (bytes_transferred) {
        size_type bytes_available = get_torrent().recv_throttler().available();

        GLOG->debug("available bytes capacity %d/%d", bytes_available, 
                m_socket.available());

        m_receive_buffer.resize(bytes_transferred);
        utils::read_available_from_socket(m_socket, 
                m_receive_buffer, bytes_available);

        get_torrent().recv_throttler().account(m_receive_buffer.size());

        GLOG->debug("got %d bytes", m_receive_buffer.size());
        m_input_message_stream.feed_data(m_receive_buffer);
    }

    if (m_input_message_stream.has_pending()) {
        process_input_messages();
    }

    setup_receive_callback();

END_METHOD


DEFINE_METHOD(void, peer_connection_t::process_input_messages)
    const std::vector<message_t> &messages = m_input_message_stream.get_pending();
    for (int i = 0; i < messages.size(); ++i) {
        GLOG->debug("got %d message-type", messages[i].type);

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
        default: 
            GLOG->warn("unknown message type %d", messages[i].type); 
            finish();
            break;
        };
    }
    m_input_message_stream.clear_pending();
END_METHOD


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
void peer_connection_t::on_have(const message_t &message) {
}
void peer_connection_t::on_bitfield(const message_t &message) {
    m_bitmap_received_callback(message.payload);
}
void peer_connection_t::on_piece(const message_t &message) {
    int index, offset;
    const u8 *p = message.payload.data();
    int4get(index, p); p += 4;
    int4get(offset, p); p += 4;
    m_piece_part_received_callback(
        index, offset / gsettings()->m_piece_part_size, 
        std::vector<u8>((u8*)&message.payload[8], 
            (u8*)&message.payload[0] + message.payload.size()));
}

void peer_connection_t::on_cancel(const message_t &message) {}

void peer_connection_t::setup_receive_callback() {
    m_receive_buffer.resize(1);
    boost::asio::async_read(m_socket,
        boost::asio::buffer(m_receive_buffer, 1),
        boost::asio::transfer_at_least(1),
        boost::bind(&peer_connection_t::on_data_received, this,
            boost::asio::placeholders::error, 
            boost::asio::placeholders::bytes_transferred));
}

DEFINE_METHOD(void, peer_connection_t::send_output_queue) 

    if (m_send_packets.empty()) {
        return;
    }

    if (m_in_flight) {
        return;
    }

    m_in_flight = true;

    std::vector<boost::asio::const_buffer> bufs;
    for (int i = 0; i < m_send_packets[0].m_bufs.size(); ++i) {
        bufs.push_back(boost::asio::buffer(m_send_packets[0].m_bufs[i]));
    }

    boost::asio::async_write(m_socket,
        bufs, boost::bind(&peer_connection_t::on_packet_sent, this,
            boost::asio::placeholders::error, 
            boost::asio::placeholders::bytes_transferred));
END_METHOD


DEFINE_METHOD(void, peer_connection_t::on_packet_sent,
        const boost::system::error_code& err, size_t bytes_transferred)

    m_in_flight = false;

    if (err == boost::asio::error::operation_aborted) {
        return;
    }

    if (err) {
        GLOG->debug("got error " + err.message());
        finish();
        return;
    }

    GLOG->debug("packet was sent %d", bytes_transferred);

    m_send_packets.pop_front(); 
    send_output_queue();

END_METHOD


DEFINE_METHOD(void, peer_connection_t::message_stream_t::feed_data,
    const std::vector<u8> &str) 

    if (str.empty())
        return;

    size_type pending_bytes = str.size();
    const u8 *p = static_cast<const u8 *>(&str[0]);

    while (pending_bytes > 0) {

    switch (m_state) {
    case s_length: {
        if (m_pending_length_count + pending_bytes >= 4) {
            memcpy(m_current.lenpart.buf + m_pending_length_count, 
                p, 4 - m_pending_length_count);
            p += 4 - m_pending_length_count;
            pending_bytes -= 4 - m_pending_length_count;
            m_pending_length_count = 4;

            int4get(m_current.lenpart.length, m_current.lenpart.buf);

            GLOG->debug("got header with %d bytes", m_current.lenpart.length);

            /* if is keepalive */
            if (m_current.lenpart.length == 0) {
                m_current.type = peer_connection_t::t_keepalive;
                m_pending_messages.push_back(m_current);
                reset();
            } else {
                m_state = s_type;

                /* taking type into account */
                --m_current.lenpart.length;
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
        message_type_t type = static_cast<message_type_t>(*p++); 
        m_current.type = type;
        --pending_bytes;

        if (m_current.lenpart.length) {
            m_state = s_payload;
        } else {
            m_pending_messages.push_back(m_current);
            reset();
        }

        break;
    }

    case s_payload: {
        std::vector<u8> &payload = m_current.payload;
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

END_METHOD

void peer_connection_t::setup_handshake_done_callback(const boost::function<void()> &cbk) {
    m_handshake_done_callback = cbk;
}

void peer_connection_t::setup_connection_lost_callback(const boost::function<void()> &cbk) {
    m_connection_lost_callback = cbk;
}

void peer_connection_t::setup_bitmap_received_callback(const 
        boost::function<void(const std::vector<u8> &)> &cbk) 
{
    m_bitmap_received_callback = cbk;
}

void peer_connection_t::setup_piece_part_received_callback(const boost::function<
    void(size_type, size_type, const std::vector<u8> &)> &cbk)
{
    m_piece_part_received_callback = cbk;
}


}
