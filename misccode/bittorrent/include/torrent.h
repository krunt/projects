#ifndef TORRENT_DEF_
#define TORRENT_DEF_

#include <include/common.h>
#include <include/torrent_storage.h>
#include <include/throttler.h>
#include <include/blocking_queue.h>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

namespace btorrent {

class torrent_t {
public:
    torrent_t(boost::asio::io_service &io_service_arg, 
            const torrent_info_t &info);

    void start();
    void finish();

    void add_peer(const std::string &peer_id, const std::string &host, int port);
    void make_request(ppeer_t peer, const piece_part_request_t &request);

    void configure_next_iteration();
    void start_tracker_connections();
    void start_statistics_reporting();
    void configure_statistics_reporting();
    void report_statistics();
    void download_iteration();

    void establish_new_active_connections();
    void establish_new_pending_connections();

    void make_piece_requests();
    void make_active_replacements();
    void dispatch_torrent_storage_responses();

    void move_to_active(ppeer_t peer);
    void move_to_pending(ppeer_t peer);
    void move_to_inactive_from_pending(ppeer_t peer);
    void move_to_inactive_from_active(ppeer_t peer);
    void move_to_blacklist(ppeer_t peer);

    void remove_unneeded_peers();
    bool remove_from_peer_list(ppeer_t peer, std::vector<ppeer_t> &list);
    peer_replacement_t remove_from_replacement_list(ppeer_t peer,
            std::vector<peer_replacement_t> &list);

    void on_connection_lost(ppeer_t peer);
    void on_bitmap_received(ppeer_t peer, const std::vector<u8> &bitmap);
    void on_piece_part_received(ppeer_t peer, size_type piece_index, 
        size_type piece_part_index, const std::vector<u8> &data);
    void on_piece_part_requested(ppeer_t peer, size_type piece_index, 
        size_type piece_part_index, std::vector<u8> &data);
    void on_aborted_request(ppeer_t peer, const piece_part_request_t &request);

    void on_torrent_storage_work_done(
            const torrent_storage_t::torrent_storage_work_t &r);

    void get_announce_urls(std::vector<url_t> &urls);

    boost::asio::io_service &io_service() { return m_io_service; }
    const sha1_hash_t &info_hash() const { return m_torrent_info.m_info_hash; }
    const std::string &peer_id() const { return m_peer_id; }
    u64 bytes_downloaded() const { return m_bytes_downloaded; }
    u64 bytes_left() const { return m_bytes_left; }
    u64 bytes_uploaded() const { return m_bytes_uploaded; }
    const torrent_info_t &get_torrent_info() const { return m_torrent_info; }

    throttler_t &send_throttler() { return m_send_throttler; }
    throttler_t &recv_throttler() { return m_recv_throttler; }

private:
    std::vector<boost::shared_ptr<btorrent::tracker_connection_t> > 
        m_tracker_connections;

    boost::shared_ptr<piece_pick_strategy_t> m_piece_pick_strategy;
    boost::shared_ptr<peer_replace_strategy_t> m_peer_replacement_strategy;

    std::vector<ppeer_t> m_unknown_peers;
    std::vector<ppeer_t> m_inactive_peers;
    std::vector<ppeer_t> m_pending_peers;
    std::vector<ppeer_t> m_active_peers;
    std::vector<ppeer_t> m_blacklist_peers;

    std::vector<peer_replacement_t> m_pending_peer_replacements;

private:
    boost::asio::io_service &m_io_service;
    std::string m_peer_id; /* my peer id */
    peer_piece_bitmap_t m_bitmap; /* my bitmap */
    u64 m_bytes_downloaded;
    u64 m_bytes_left;
    u64 m_bytes_uploaded;
    torrent_info_t m_torrent_info;
    torrent_storage_t m_torrent_storage;
    boost::thread m_torrent_storage_thread;

    boost::mutex m_torrent_storage_responses_guard;
    std::deque<torrent_storage_t::torrent_storage_work_t> 
        m_torrent_storage_responses;

    int m_download_time_interval;
    boost::asio::basic_deadline_timer<boost::posix_time::ptime> 
        m_timeout_timer;
    boost::asio::basic_deadline_timer<boost::posix_time::ptime> 
        m_statistics_timer;

    throttler_t m_send_throttler;
    throttler_t m_recv_throttler;

    /* keep it last for now */
    std::map<std::string, boost::shared_ptr<peer_t> > m_peerid2peer_map;
};

}


#endif /* TORRENT_DEF_ */
