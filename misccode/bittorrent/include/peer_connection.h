#ifndef PEER_CONNECTION_DEF_
#define PEER_CONNECTION_DEF_

#include <include/common.h>
#include <include/torrent.h>

#include <deque>

#include <boost/function.hpp>

namespace btorrent {

class peer_connection_t {
public:
    peer_connection_t(torrent_t &torrent, 
            const std::string &peer_id,
            const std::string &host, int port)
        : m_torrent(torrent), m_resolver(m_torrent.io_service()), 
        m_socket(m_torrent.io_service()), m_peer_id(peer_id), 
        m_host(host), m_port(port), m_state(k_none),
        m_wait_available_send_timer(m_torrent.io_service()),
        m_wait_available_recv_timer(m_torrent.io_service())
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
    void setup_piece_part_requested_callback(const boost::function<
            void(size_type, size_type)> &cbk);

    void send_keepalive(bool async = false);
    void send_choke(bool async = false);
    void send_unchoke(bool async = false);
    void send_interested(bool async = false);
    void send_notinterested(bool async = false);
    void send_have(u32 index, bool async = false);
    /* void send_bitfield(torrent_t::bitfield_type &t); */
    void send_request(u32 index, u32 begin, u32 length, bool async = false);
    void send_piece(u32 index, u32 begin, const std::vector<u8> &data, 
            bool async = false);

    void send_output_queue();

private:
    void on_resolve(const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void on_connect(const boost::system::error_code& err);
    void on_handshake_response(const boost::system::error_code& err, 
            size_t bytes_transferred);
    void on_packet_sent(const boost::system::error_code& err, 
            size_t bytes_transferred);
    void perform_handshake();

    enum message_type_t { t_keepalive = -1, t_choke = 0, t_unchoke, t_interested,
        t_not_interested, t_have, t_bitfield, t_request, t_piece, t_cancel };
    struct message_t {
        union {
            u32 length;
            u8 buf[4];
        } lenpart;

        message_type_t type;
        std::vector<u8> payload; 
    };

    void on_keepalive(const message_t &message);
    void on_choke(const message_t &message);
    void on_unchoke(const message_t &message);
    void on_interested(const message_t &message);
    void on_notinterested(const message_t &message);
    void on_have(const message_t &message);
    void on_bitfield(const message_t &message);
    void on_piece(const message_t &message);
    void on_request(const message_t &message);
    void on_cancel(const message_t &message);

    void on_data_received(const boost::system::error_code& err, 
            size_t bytes_transferred);

    void send_common(message_type_t type, const std::string &payload, 
            bool async = false);

    void process_input_messages();

    bool setup_send_callback();
    void setup_receive_callback();

    void on_send_wait_timer_expired_cb();
    void on_receive_wait_timer_expired_cb();

    void configure_send_wait_timer();
    void configure_receive_wait_timer();

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
    boost::function<void(size_type, size_type)> m_piece_part_requested_callback;

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

    struct packet_t {
        std::vector<std::string> m_bufs;
    };

    bool m_in_flight;
    std::deque<packet_t> m_send_packets;

    boost::asio::basic_deadline_timer<boost::posix_time::ptime> 
        m_wait_available_send_timer;
    boost::asio::basic_deadline_timer<boost::posix_time::ptime> 
        m_wait_available_recv_timer;
};

}

#endif /* PEER_CONNECTION_DEF_ */
