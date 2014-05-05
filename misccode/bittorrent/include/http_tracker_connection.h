#ifndef HTTP_TRACKER_CONNECTION_DEF_
#define HTTP_TRACKER_CONNECTION_DEF_

#include <include/common.h>

namespace btorrent {

class http_tracker_connection_t: public tracker_connection_t {
public:
    http_tracker_connection_t(torrent_t &torrent, 
            const url_t &announce_url);

    void start();
    void finish();

private:
    void on_resolve(const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void on_connect(const boost::system::error_code& err,
         boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void on_received_connect_response(const boost::system::error_code& err, 
        size_t bytes_transferred);
    void on_received_announce_response(
        const boost::system::error_code& err, size_t bytes_transferred);

private:
    static const int k_max_buffer_size = 4096;
    enum state_t { s_none = 0, s_announcing = 1, };

    std::string m_host;
    int m_port; 
    std::string m_path;

    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::ip::tcp::socket m_socket;

    std::vector<u8> m_data;
    state_t m_state;
};

}


#endif /* HTTP_TRACKER_CONNECTION_DEF_ */
