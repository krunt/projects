#ifndef TORRENT_DEF_
#define TORRENT_DEF_

#include <include/common.h>
#include <boost/asio.hpp>

namespace btorrent {

class torrent_t {
public:
    torrent_t(boost::asio::io_service &io_service_arg, const torrent_info_t &info);

    void start();
    void finish();

    boost::asio::io_service &io_service() { return m_io_service; }

    void add_peer(const std::string &peer_id, const std::string &host, int port);
    void make_request(ppeer_t peer, const piece_part_request_t &request);

    void move_to_active(ppeer_t peer);
    void move_to_pending(ppeer_t peer);
    void move_to_inactive_from_pending(ppeer_t peer);
    void move_to_inactive_from_active(ppeer_t peer);
    void move_to_blacklist(ppeer_t peer);

    void on_connection_lost(ppeer_t peer);
    void on_bitmap_received(ppeer_t peer, const std::vector<u8> &bitmap);
    void on_piece_part_received(ppeer_t peer, size_type piece_index, 
        size_type piece_part_index, const std::vector<u8> &data);
    void on_aborted_request(ppeer_t peer, const piece_part_request_t &request);

    void get_announce_urls(std::vector<url_t> &urls);
    const sha1_hash_t &info_hash() const { return m_torrent_info.m_info_hash; }
    const std::string &peer_id() const { return m_peer_id; }
    u64 bytes_downloaded() const { return m_bytes_downloaded; }
    u64 bytes_left() const { return m_bytes_left; }
    u64 bytes_uploaded() const { return m_bytes_uploaded; }
    const torrent_info_t &get_torrent_info() const { return m_torrent_info; }

private:

    struct peer_replacement_t {
        peer_replacement_t(const std::string &from, const std::string &to)
            : m_from_peer_id(from), m_to_peer_id(to)
        {}

        std::string m_from_peer_id;
        std::string m_to_peer_id;
    };

    boost::shared_ptr<piece_pick_strategy_t> m_piece_pick_strategy;
    boost::shared_ptr<peer_replace_strategy_t> m_peer_replacement_strategy;

    typedef boost::shared_ptr<peer_t> ppeer_t;
    std::vector<ppeer_t> m_unknown_peers;
    std::vector<ppeer_t> m_inactive_peers;
    std::vector<ppeer_t> m_pending_peers;
    std::vector<ppeer_t> m_active_peers;
    std::vector<ppeer_t> m_blacklist_peers;

    std::map<std::string, ppeer_t> m_peerid2peer_map;

    std::vector<peer_replacement_t> m_pending_peer_replacements;

private:
    boost::asio::io_service &m_io_service;
    std::string m_peer_id; /* my peer id */
    peer_piece_bitmap_t m_bitmap; /* my bitmap */
    u64 m_bytes_downloaded;
    u64 m_bytes_left;
    u64 m_bytes_uploaded;
    torrent_info_t m_torrent_info;
};

}


#endif /* TORRENT_DEF_ */
