#ifndef UDP_TRACKER_CONNECTION_DEF_
#define UDP_TRACKER_CONNECTION_DEF_

#include <include/common.h>
#include <include/torrent.h>
#include <include/tracker_connection.h>

#include <boost/asio.hpp>
#include <boost/date_time.hpp>

namespace btorrent {

class udp_tracker_connection_t: public tracker_connection_t {
public:
    udp_tracker_connection_t(torrent_t &torrent, 
            const url_t &announce_url);

    void start();
    void finish();

private:
    void on_resolve(const boost::system::error_code& err,
        boost::asio::ip::udp::resolver::iterator endpoint_iterator);
    void on_connect(const boost::system::error_code& err);
    void on_received_connect_response(const boost::system::error_code& err, 
        size_t bytes_transferred);
    void on_received_announce_response(
        const boost::system::error_code& err, size_t bytes_transferred);

    /* timeout stuff */
    void on_timer_timeout();
    void configure_timeout();
    void cancel_timeout();
    int timeout_in_seconds() const { return 15 * (1 << m_timeout_step); }

private:
    static const int k_max_buffer_size = 4096;
    enum action_t { k_connect = 0, k_announce = 1, };
    enum state_t { s_none = 0, s_announcing = 1, };

    std::string m_host;
    int m_port; 

    boost::asio::ip::udp::resolver m_resolver;
    boost::asio::ip::udp::socket m_socket;

    u32 m_transaction_id;
    u64 m_connection_id;

    std::vector<u8> m_send_buffer;
    std::vector<u8> m_data;
    state_t m_state;

    int m_timeout_step;
    boost::asio::basic_deadline_timer<boost::posix_time::ptime> 
        m_timeout_timer;
};

}


#endif /* UDP_TRACKER_CONNECTION_DEF_ */
