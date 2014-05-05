
#include <include/common.h>

#include <boost/asio/ip/address_v4.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <include/http_tracker_connection.h>
#include <include/torrent.h>

#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"
#define REQUEST_FORMAT_STR \
    "GET %s HTTP/1.1" CRLF \
    "Host: %s:%d" CRLF \
    "User-Agent: tiny_krunt_client" CRLF \
    CRLF

namespace ip = boost::asio::ip;

namespace btorrent {

http_tracker_connection_t::http_tracker_connection_t(torrent_t &torrent,
        const url_t &announce_url)
    : tracker_connection_t(torrent), m_host(announce_url.get_host()), 
      m_port(announce_url.get_port()), m_path(announce_url.get_path()),
      m_resolver(get_torrent().io_service()),
      m_socket(get_torrent().io_service())
{}

DEFINE_METHOD(void, http_tracker_connection_t::start)
    m_state = s_announcing;

    m_data.resize(k_max_buffer_size);
    ip::tcp::resolver::query query(m_host, 
            boost::lexical_cast<std::string>(m_port));

    GLOG->debug("trying to connect to %s:%d", 
            m_host.c_str(), m_port);

    m_resolver.async_resolve(query,
        boost::bind(&http_tracker_connection_t::on_resolve, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));
END_METHOD

DEFINE_METHOD(void, http_tracker_connection_t::finish)
    GLOG->debug("%s/%d", m_host.c_str(), m_port);

    m_socket.close();

    m_state = s_none;
END_METHOD


DEFINE_METHOD(void, http_tracker_connection_t::on_resolve,
    const boost::system::error_code& err,
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    if (err) {
        GLOG->error("got error: " + err.message());
        finish();
        return;
    }

    GLOG->debug("resolved successfully to %s:%d", (*endpoint_iterator)
        .endpoint().address().to_string().c_str(), m_port);

    boost::asio::ip::tcp::endpoint endpoint(*endpoint_iterator);
    m_socket.async_connect(endpoint,
        boost::bind(&http_tracker_connection_t::on_connect, this,
            boost::asio::placeholders::error,
            ++endpoint_iterator));

END_METHOD


DEFINE_METHOD(void, http_tracker_connection_t::on_connect, 
        const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator)

    if (err) {
        if (endpoint_iterator == boost::asio::ip::tcp::resolver::iterator()) {
            GLOG->error("got error: " + err.message());
            finish();
            return;
        }

        m_socket.close();
        boost::asio::ip::tcp::endpoint endpoint(*endpoint_iterator);
        m_socket.async_connect(endpoint,
            boost::bind(&http_tracker_connection_t::on_connect, this,
                boost::asio::placeholders::error,
                ++endpoint_iterator));
        return;
    }

    char buf[4096]; int buf_length;
    std::string request_str = m_path
        + (m_path.find('?') != std::string::npos ? "&" : "?");

    request_str += "info_hash=" + utils::escape_http_string(
            get_torrent().info_hash().get_digest());
    request_str += "&peer_id=" + utils::escape_http_string(
            get_torrent().peer_id());
    request_str += "&port=6881" ; /* TODO */
    request_str += "&uploaded=" 
        + boost::lexical_cast<std::string>(get_torrent().bytes_uploaded());
    request_str += "&downloaded=" 
        + boost::lexical_cast<std::string>(get_torrent().bytes_downloaded());
    request_str += "&left=" 
        + boost::lexical_cast<std::string>(get_torrent().bytes_left());
    buf_length = snprintf(buf, sizeof(buf), REQUEST_FORMAT_STR, request_str.c_str(),
        m_host.c_str(), m_port);

    GLOG->debug("sending http-announce-request");

    if (m_socket.send(boost::asio::buffer(buf, buf_length)) != buf_length)
    {
        GLOG->error("got announce-request error");
        finish();
        return;
    }

    GLOG->debug("sending announce-request done.");

    m_socket.async_receive(boost::asio::buffer(m_data, k_max_buffer_size),
        boost::bind(&http_tracker_connection_t::on_received_announce_response, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));

END_METHOD


DEFINE_METHOD(void, http_tracker_connection_t::on_received_announce_response,
        const boost::system::error_code& err, size_t bytes_transferred)

    if (err) { 
        GLOG->error("got error: " + err.message());
        finish();
        return; 
    }

    m_data.resize(bytes_transferred);
    utils::read_available_from_socket(m_socket, m_data);

    GLOG->debug("got %d bytes", m_data.size());

    try { 
        value_t decoded_bep = bdecode(
            std::string(reinterpret_cast<char*>(&m_data[0]), m_data.size()));

        GLOG->debug("got bencoded response");
        if (decoded_bep.exists("failure reason")) {
            GLOG->warn("got failure reason from bep: %s", 
                    decoded_bep["failure reason"].to_string().c_str());
        } else {
            value_t::integer_type interval = decoded_bep["interval"].to_int();
            value_t::list_type &peer_list = decoded_bep["peers"].to_list();
            for (int i = 0; i < peer_list.size(); ++i) {
                get_torrent().add_peer(peer_list[i]["peer id"].to_string(), 
                    peer_list[i]["ip"].to_string(), peer_list[i]["port"].to_int());
            }
        }
    } catch (const bad_encode_decode_exception &exc) {
        GLOG->debug("got compact list");

        /* it is compact list */
        u8 *p = &m_data[0]; size_t left_bytes = m_data.size();
        while (left_bytes >= 6) {
            u32 ip_address; u16 ip_port;

            int4get(ip_address, p); p += 4;
            int2get(ip_port, p); p += 2;

            get_torrent().add_peer(utils::ipv4_to_string(ip_address), 
                    utils::ipv4_to_string(ip_address), ip_port);
            
            left_bytes -= 6;
        }
    }

    finish();

END_METHOD

}
