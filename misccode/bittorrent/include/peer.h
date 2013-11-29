#ifndef PEER_DEF_
#define PEER_DEF_

#include <boost/noncopyable.hpp>

namespace btorrent {

class peer_t: public boost::noncopyable {
public:
    enum state_t { 
        s_none,

        s_bitmap_pending, /* move to s_bitmap_done after getting bitmap */
        s_bitmap_done, 

        s_bitmap_active_pending, /* move to s_active after getting bitmap */
        s_active,

        s_finishing_from_active,
        s_finishing_from_pending,

        s_in_replacement,

        s_blacklist, /* move to s_blacklist after invalid hack-check */
    };

public:
    peer_t(torrent_t &torrent, const std::string &peer_id, 
            const std::string &host, int port)
        : m_state(s_none), m_conn(torrent, host, port), m_torrent(torrent)
    {}

    void start_active();
    void start_pending();
    void make_request(const piece_part_request_t &request);
    void finish();
    void finish_with_replacement();

    const std::string &id() const { return m_peer_id; }
    const state_t get_state() const { return m_state; }
    const peer_piece_bitmap_t &get_bitmap() const { return m_bitmap; }
    bool is_finishing() const { 
        return m_state == s_finishing_from_active
            || m_state == s_finishing_from_pending;
    }

private:
    void setup_callbacks();

    /* connection callbacks */
    void on_handshake_done();
    void on_connection_lost();
    void on_bitmap_received(const std::vector<u8> &bitmap);
    void on_piece_part_received(size_type piece_index, 
        size_type piece_part_index, const std::vector<u8> &data);

    /* other stuff */
    void remove_from_pending(size_type piece_index, size_type piece_part_index);

private:
    state_t m_state;
    std::string m_peer_id;
    peer_piece_bitmap_t m_bitmap;
    peer_connection_t m_conn;
    std::vector<piece_part_request_t> m_pending_requests;
    torrent_t &m_torrent;
};

typedef peer_t *ppeer_t;

}

#endif /* PEER_DEF_ */
