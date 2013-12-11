#include <include/torrent.h>
#include <include/peer.h>

namespace btorrent {

torrent_t::torrent_t(boost::asio::io_service &io_service_arg, 
        const torrent_info_t &info)
    : m_io_service(io_service_arg), 
    m_peer_id(generate_random(20)), m_bitmap(m_peer_id, info.m_piece_hashes.size(),
        info.m_piece_size / gsettings()->m_piece_part_size),
    m_bytes_downloaded(0), m_bytes_left(0),
    m_bytes_uploaded(0), m_torrent_info(info), m_torrent_storage(*this),
    m_timeout_timer(m_io_service)
{
    /* computing total file size */
    for (int i = 0; i < m_torrent_info.m_files.size(); ++i) {
        m_bytes_left += m_torrent_info.m_files[i].m_size;
    }

    m_piece_pick_strategy.reset(create_piece_pick_strategy(
        gsettings()->m_piece_pick_strategy_type, m_bitmap));
    m_peer_replacement_strategy.reset(create_peer_replace_strategy(
        gsettings()->m_peer_replace_strategy_type, m_bitmap));
}

DEFINE_METHOD(void, torrent_t::add_peer, const std::string &peer_id, 
        const std::string &host, int port) 

    GLOG->info("got %s:%d", host.c_str(), port); 
    m_peerid2peer_map[peer_id] 
        = boost::shared_ptr<peer_t>(new peer_t(*this, peer_id, host, port));
    m_unknown_peers.push_back(m_peerid2peer_map[peer_id].get());

END_METHOD

void torrent_t::configure_next_iteration() {
    m_timeout_timer.expires_from_now(boost::posix_time::seconds(
                m_download_time_interval));
    m_timeout_timer.async_wait(boost::bind(&torrent_t::download_iteration, this));
}

void torrent_t::get_announce_urls(std::vector<url_t> &urls) {
    urls.push_back(url_t(m_torrent_info.m_announce_url));
    for (int i = 0; i < m_torrent_info.m_announce_list.size(); ++i) {
        urls.push_back(url_t(m_torrent_info.m_announce_list[i]));
    }
}

void torrent_t::start_tracker_connections() {
    std::vector<url_t> urls;
    get_announce_urls(urls);

    m_tracker_connections.clear();
    for (int i = 0; i < urls.size(); ++i) {
        m_tracker_connections.push_back(
            btorrent::tracker_connection_factory_t::construct(torrent, url));
        m_tracker_connections.back()->start();
    }
}

void torrent_t::start() {
    start_tracker_connections();
    download_iteration();
}

void torrent_t::finish() {
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
                ppeer_t peer = m_peerid2peer_map[peer_id].get();
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
        ppeer_t peer = m_peerid2peer_map[request.m_peer_id].get();
        make_request(peer, request);
    }
}

void torrent_t::remove_unneeded_peers() {
    std::vector<std::string unneeded_peers;
    m_peer_replacement_strategy->get_peers_to_remove(unneeded_peers);

    for (int i = 0; i < unneeded_peers.size(); ++i) {
        const std::string &peer_id = unneeded_peers[i];
        ppeer_t peer = m_peerid2peer_map[peer_id].get();
        if (!peer->is_finishing()
            && peer->get_state() != peer_t::s_in_replacement) 
        {
            peer->finish();
        }
    }

    /* removing unneeded from pending */
    std::vector<ppeer_t> pending_peers_copy(m_pending_peers);
    for (int i = 0; i < pending_peers_copy.size(); ++i) {
        ppeer_t peer = pending_peers_copy[i];
        if (peer->get_state() == peer_t::s_bitmap_done) {
            peer->finish();
        }
    }
}

void torrent_t::make_active_replacements() {
    std::vector<peer_replacement_t> replacements;
    m_peer_replacement_strategy->get_peer_replacements(replacements);

    for (int i = 0; i < replacements.size(); ++i) {
        ppeer_t peer = m_peerid2peer_map[replacements[i].m_from_peer_id].get();
        if (peer->get_state() != peer_t::s_in_replacement
                && !peer->is_finishing())
        {
            peer->finish_with_replacement();
            m_pending_peer_replacements.push_back(peer_replacement_t(from, to));
        }
    }
}

void torrent_t::download_iteration() {
    establish_new_active_connections();
    establish_new_pending_connections();
    make_piece_requests();
    remove_unneeded_peers();
    make_active_replacements();

    configure_next_iteration();
}

void torrent_t::make_request(ppeer_t peer, const piece_part_request_t &request) {
    m_piece_pick_strategy->on_piece_part_requested(request);
    m_peer_replacement_strategy->on_piece_part_requested(request);
    m_bitmap.set_piece_part(request.piece_index, request.piece_part_index, 
        peer_piece_bitmap_t::pp_requested);
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
    remove_from_list(peer, m_pending_peers);
    m_inactive_peers.push_back(peer);
}

void torrent_t::move_to_inactive_from_active(ppeer_t peer) {
    m_peer_replacement_strategy->on_inactive_peer_added(peer->get_bitmap());
    remove_from_list(peer, m_active_peers);
    m_inactive_peers.push_back(peer);
}

void torrent_t::move_to_blacklist(ppeer_t peer) {
    m_blacklist_peers.push_back(peer);
}

bool torrent_t::remove_from_list(ppeer_t peer, std::vector<ppeer_t> &list) {
    bool removed = false;
    std::vector<ppeer_t> new_list;
    for (int i = 0; i < list.size(); ++i) {
        if (peer != list[i]) {
            new_list.push_back(list[i]);
        } else {
            removed = true;
        }
    }
    list.swap(new_list);
    return removed;
}

bool torrent_t::remove_from_replacement_list(ppeer_t peer, 
        std::vector<peer_replacement_t> &list) 
{
    peer_replacement_t result("", "");
    std::vector<peer_replacement_t> new_list;
    for (int i = 0; i < list.size(); ++i) {
        if (peer->id() != list[i].m_from_peer_id) {
            new_list.push_back(list[i]);
        } else {
            result = peer;
        }
    }
    list.swap(new_list);
    return result;
}

void torrent_t::on_bitmap_received(ppeer_t peer, const std::vector<u8> &bitmap) {
    if (peer->get_state() == s_active) {
        m_piece_pick_strategy->on_peer_added(peer->get_bitmap());
        m_peer_replacement_strategy->on_active_peer_added(peer->get_bitmap());
    } else if (peer->get_state() == s_bitmap_done) {
        m_peer_replacement_strategy->on_inactive_peer_added(peer->get_bitmap());
    }
}

void torrent_t::on_piece_part_received(ppeer_t peer, size_type piece_index,
    size_type piece_part_index, const std::vector<u8> &data) 
{
    assert(peer->get_state() == s_active);
    piece_part_request_t piece_request(peer->id(), piece_index, piece_part_index);
    m_piece_pick_strategy->on_piece_part_downloaded(piece_request);
    m_peer_replacement_strategy->on_piece_part_downloaded(piece_request);

    m_bitmap.set_piece_part(piece_index, piece_part_index, 
        peer_piece_bitmap_t::pp_downloaded);

    m_torrent_storage.add_piece_part(piece_index, piece_part_index, data);

    if (m_bitmap.is_piece_downloaded(piece_index)) {
        if (m_torrent_storage.validate_piece(piece_index)) {
            m_bitmap.set_piece(piece_index);
            m_piece_pick_strategy->on_piece_validation_done(piece_index);
            m_peer_replacement_strategy->on_piece_validation_done(piece_index);
        } else {
            m_bitmap.unset_piece(piece_index);
            m_piece_pick_strategy->on_piece_validation_failed(piece_index);
            m_peer_replacement_strategy->on_piece_validation_failed(piece_index);
        }
    }
}

void torrent_t::on_aborted_request(ppeer_t peer, 
    const piece_part_request_t &request) 
{
    m_piece_pick_strategy->on_piece_part_aborted(peer->get_bitmap());
    m_peer_replacement_strategy->on_piece_part_aborted(peer->get_bitmap());
    m_bitmap.set_piece_part(request.m_piece_index,
        request.m_piece_part_index, peer_piece_bitmap_t::pp_none);
}

void torrent_t::on_connection_lost(ppeer_t peer) {
    assert(peer->is_finishing() || peer->get_state() == s_in_replacement);

    if (peer->get_state() == s_in_replacement
        || peer->get_state() == s_finishing_from_active)
    {
        move_to_inactive_from_active(peer); 
        m_piece_pick_strategy->on_peer_removed(peer->get_bitmap());
        m_peer_replacement_strategy->on_active_peer_removed(peer->get_bitmap());
    }

    if (peer->get_state() == s_finishing_from_pending) {
        move_to_inactive_from_pending(peer);

    } else if (peer->get_state() == s_in_replacement) {
        /* perform replacement */
        assert(!m_pending_peer_replacements.empty());
        peer_replacement_t replacement
            = remove_from_replacement_list(peer, m_pending_peer_replacements);

        assert(!replacement.empty());

        /* establish connection with new peer id */
        ppeer_t to_peer = m_peerid2peer_map[replacement.m_to_peer_id].get();
        to_peer->start_active();
        move_to_active(to_peer);
    } 
}

}

