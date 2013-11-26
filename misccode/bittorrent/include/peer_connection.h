#ifndef TRACKER_CONNECTION_DEF_
#define TRACKER_CONNECTION_DEF_

#include <include/common.h>
#include <include/torrent.h>

#include <boost/function.hpp>

namespace btorrent {

class peer_connection_t {
public:
    peer_connection_t(torrent_t &torrent, 
            const std::string &peer_id,
            const std::string &host, int port)
        : m_torrent(torrent), m_peed_id(peer_id), 
          m_host(host), m_port(port)
    {}

    virtual void start() = 0;
    virtual void finish() = 0;

    torrent_t &get_torrent() { return m_torrent; }
    const torrent_t &get_torrent() const { return m_torrent; }

private:
    void on_resolve(const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void on_connect(const boost::system::error_code& err);
    void perform_handshake();

    enum message_type_t { t_keepalive = -1, t_choke = 0, t_unchoke, t_interested,
        t_not_interested, t_have, t_bitfield, t_request,
        t_request, t_piece, t_cancel };
    void send_common(message_type_t type, const std::string &payload);

private:
    enum state_t { k_none = 0, k_connecting = 1, 
        k_connected = 2, k_active = 3, };

    torrent_t &m_torrent;

    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::ip::tcp::socket m_socket;

    std::string m_peer_id;
    std::string m_host;
    int m_port;

    state_t m_state;

    static const int k_max_buffer_size = 4096;
    std::vector<u8> m_receive_buffer;

    struct message_t {
        union {
            u32 length;
            u8 buf[4];
        } lenpart;

        message_type_t type;
        std::vector<u8> payload; 
    };

    class message_stream_t {
    public:
        void feed_data(const std::vector<u8> &str);

        const std::vector<message_t> &get_pending() const { 
            return m_pending_messages; 
        }

        bool has_pending() const { return !m_pending_messages.empty(); }
        void clear_pending() { m_pending_messages.clear(); }

    private:
        size_type m_pending_bytes_count;
        char m_pending_bytes[8];

        std::vector<message_t> m_pending_messages;
    };

    message_stream_t m_input_message_stream;
};

}

#endif /* TRACKER_CONNECTION_DEF_ */
