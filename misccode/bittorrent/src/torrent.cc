#include <include/torrent.h>

namespace btorrent {

torrent_t::torrent_t(boost::asio::io_service &io_service_arg, 
        const torrent_info_t &info)
    : m_io_service(io_service_arg), m_bytes_downloaded(0), m_bytes_left(0),
    m_bytes_uploaded(0), m_torrent_info(info)
{
    /* computing unique peer_id */
    char c[20];
    btorrent::generate_random(c, sizeof(c));
    m_peer_id = std::string(c, c + 20);

    /* computing total file size */
    for (int i = 0; i < m_torrent_info.m_files.size(); ++i) {
        m_bytes_left += m_torrent_info.m_files[i].m_size;
    }
}

DEFINE_METHOD(void, torrent_t::add_peer, const std::string &host, int port) 
    GLOG->info("got %s:%d", host.c_str(), port); 
END_METHOD

void torrent_t::get_announce_urls(std::vector<url_t> &urls) {
    urls.push_back(url_t(m_torrent_info.m_announce_url));
    for (int i = 0; i < m_torrent_info.m_announce_list.size(); ++i) {
        urls.push_back(url_t(m_torrent_info.m_announce_list[i]));
    }
}

void torrent_t::start() {
}

void torrent_t::establish_new_active_connections() {
    int active_count = gsettings()->m_max_active_connections; 

    while (m_active_peers.size() < active_count) {

        /* for now pick random from unknown,
         * TODO need more clear approach */

        if (!m_unknown_peers.empty()) {
            ppeer_t peer = utils::pop_random(m_unknown_peers);
            peer->start_active();
            move_to_active(peer);
        } else {
            std::string peer_id;
            if (m_peer_replacement_strategy->get_peer_to_add(peer_id)) {
                ppeer_t peer = m_peerid2peer_map[peer_id];
                peer->start_active();
                move_to_active(peer);
            } else {

                /* nothing to do, we are done */
                break;
            }
        }
    }
}

void torrent_t::establish_new_pending_connections() {
    int check_count = gsettings()->m_max_check_connections;
    while (m_pending_peers.size() < check_count) {

        if (!m_unknown_peers.empty()) {
            ppeer_t peer = utils::pop_random(m_unknown_peers);
            peer->start_pending();
            move_to_pending(peer);
        } else {
            /* nothing to do, we are done */
            break;
        }
    }
}

void torrent_t::make_piece_requests() {
    piece_part_request_t request;
    while (m_piece_pick_strategy->get_next_part(request)) {
        ppeer_t &peer = m_peerid2peer_map[request.m_peer_id];
        make_request(peer, request);
    }
}

void torrent_t::remove_unneeded_peers() {
    std::string peer_id;
    while (m_peer_replacement_strategy->get_peer_to_remove(peer_id)) {
        ppeer_t peer = m_peerid2peer_map[peer_id];
        peer->finish();
    }
}

void torrent_t::make_active_replacements() {
    std::string from, to;
    while (m_peer_replacement_strategy->get_peer_replacement(from, to)) {
        ppeer_t peer = m_peerid2peer_map[peer_id];
        peer->finish();
        m_pending_peer_replacements.push_back(peer_replacement_t(from, to));
    }
}

void torrent_t::download_iteration() {
    establish_new_active_connections();
    establish_new_pending_connections();
    make_piece_requests();
    remove_unneeded_peers();
    make_active_replacements();
}

void torrent_t::make_request(ppeer_t peer, const piece_part_request_t &request) {
    m_piece_pick_strategy->on_piece_part_requested(request);
    m_peer_replacement_strategy->on_piece_part_requested(request);
    peer->make_request(request);
}

void torrent_t::move_to_active(ppeer_t peer) {
    m_active_peers.push_back(peer);
}

void torrent_t::move_to_pending(ppeer_t peer) {
    m_pending_peers.push_back(peer);
}

void torrent_t::move_to_inactive_from_pending(ppeer_t peer) {
    m_peer_replacement_strategy->on_inactive_peer_added(peer->get_bitmap());
    m_inactive_peers.push_back(peer);
}

void torrent_t::move_to_inactive_from_active(ppeer_t peer) {
    m_peer_replacement_strategy->on_inactive_peer_added(peer->get_bitmap());
    m_inactive_peers.push_back(peer);
}

void torrent_t::move_to_blacklist(ppeer_t peer) {
    m_blacklist_peers.push_back(peer);
}

void torrent_t::on_bitmap_received(ppeer_t peer, const std::vector<u8> &bitmap) {
    if (peer->m_state == s_active) {
        m_piece_pick_strategy->on_peer_added(peer->get_bitmap());
        m_peer_replacement_strategy->on_active_peer_added(peer->get_bitmap());
    } else if (peer->m_state == s_bitmap_done) {
        m_peer_replacement_strategy->on_inactive_peer_added(peer->get_bitmap());
    }
}

void torrent_t::on_aborted_request(ppeer_t peer, 
    const piece_part_request_t &request) 
{
    m_piece_pick_strategy.on_piece_part_aborted(peer->get_bitmap());
    m_peer_replacement_strategy->on_piece_part_aborted(peer->get_bitmap());
}

void torrent_t::on_connection_lost(ppeer_t peer) {
    if (peer->s_active) {
        move_to_inactive_from_active(peer); 
        m_piece_pick_strategy->on_peer_removed(peer->get_bitmap());
        m_peer_replacement_strategy->on_inactive_peer_removed(peer->get_bitmap());
    }
}

}
