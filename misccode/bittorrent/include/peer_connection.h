#ifndef PEER_CONNECTION_DEF_
#define PEER_CONNECTION_DEF_

#include <include/common.h>
#include <include/torrent.h>

#include <boost/function.hpp>

namespace btorrent {

class peer_connection_t {
public:
    peer_connection_t(torrent_t &torrent, 
            const std::string &peer_id,
            const std::string &host, int port)
        : m_torrent(torrent), m_resolver(m_torrent.io_service()), 
        m_socket(m_torrent.io_service()), m_peer_id(peer_id), 
        m_host(host), m_port(port)
    {}

    void start();
    void finish();

    torrent_t &get_torrent() { return m_torrent; }
    const torrent_t &get_torrent() const { return m_torrent; }

    /* set up callbacks part */
    void setup_handshake_done_callback(const boost::function<void()> &cbk);
    void setup_connection_lost_callback(const boost::function<void()> &cbk);
    void setup_bitmap_received_callback(const 
        boost::function<void(const std::vector<u8> &)> &cbk);
    void setup_piece_part_received_callback(const boost::function<
            void(size_type, size_type, const std::vector<u8> &)> &cbk);

private:
    void on_resolve(const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void on_connect(const boost::system::error_code& err);
    void perform_handshake();

    enum message_type_t { t_keepalive = -1, t_choke = 0, t_unchoke, t_interested,
        t_not_interested, t_have, t_bitfield, t_request, t_piece, t_cancel };
    void send_common(message_type_t type, const std::string &payload);

    void process_input_messages();
    void setup_receive_callback();

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

    /* callbacks */
    boost::function<void()> m_handshake_done_callback;
    boost::function<void()> m_connection_lost_callback;
    boost::function<void(const std::vector<u8> &)> m_bitmap_received_callback;
    boost::function<void(size_type, size_type, 
        const std::vector<u8> &)> m_piece_part_received_callback;

    struct connection_state_t {
        connection_state_t()
            : me_interested(false), 
              he_interested(false),
              me_choked(true),
              he_choked(false)
        {}

        bool me_interested;
        bool he_interested;

        bool me_choked;
        bool he_choked;
    };

    connection_state_t m_connection_state;

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
        message_stream_t() { reset(); }

        void feed_data(const std::vector<u8> &str);
        const std::vector<message_t> &get_pending() const { 
            return m_pending_messages; 
        }
        bool has_pending() const { return !m_pending_messages.empty(); }
        void clear_pending() { m_pending_messages.clear(); }

        void reset() { 
            m_pending_length_count = 0;
            m_state = s_length;
            m_current.payload.clear();
        }

    private:
        enum state_t { s_length = 0, s_type = 1, s_payload = 2, };

        state_t m_state;
        size_type m_pending_length_count;

        message_t m_current;
        std::vector<message_t> m_pending_messages;
    };

    message_stream_t m_input_message_stream;
};

}

#endif /* PEER_CONNECTION_DEF_ */