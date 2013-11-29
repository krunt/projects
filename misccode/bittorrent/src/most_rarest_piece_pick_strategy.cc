
namespace btorrent {

most_rarest_piece_pick_strategy_t::most_rarest_piece_pick_strategy_t(
    const peer_piece_bitmap_t &my_bitmap)
    : m_mybitmap(my_bitmap), 
      m_max_pending_requests_per_peer(gsettings()->m_max_pending_requests_per_peer)
{
    m_cumulative_piece_count.resize(m_mybitmap.piece_count(), 0);
    m_peer_per_piece_list.resize(m_mybitmap.piece_count());
}

bool most_rarest_piece_pick_strategy_t::get_next_part(
    piece_part_request_t &req) 
{
    peer_piece_bitmap_iterator_t it(m_mybitmap, 0 /* not done */);

    std::vector<size_type> need_pieces;
    for (; !it.at_end(); ++it) {
        if (m_cumulative_piece_count[*it]) {
            need_pieces.push_back(*it);
        }
    }

    std::sort(need_pieces.begin(), need_pieces.end(), 
        piece_sorter_t(m_cumulative_piece_count));

    for (int i = 0; i < need_pieces.size(); ++i) {
        size_type piece_index = need_pieces[i];

        const peer_list_per_piece_t &peer_list 
            = m_peer_per_piece_list[piece_index];

        peer_piece_part_iterator_t it(m_mybitmap,
            piece_index, peer_piece_bitmap_t::pp_none);
        for (; !it.at_end(); ++it) {
            for(peer_list_per_piece_t::const_iterator it = peer_list.begin();
                    it != peer_list.end(); ++it) 
            {
                if (it->m_pending_count < m_max_pending_requests_per_peer) {

                    req.m_peer_id = it->m_bitmap.peer_id();
                    req.m_piece_index = piece_index;
                    req.m_piece_part_index = *it;

                    return true;
                }
            }
        }
    }

    /* none found */
    return false;
}

void most_rarest_piece_pick_strategy_t::on_peer_added(
    const peer_piece_bitmap_t &bitmap)
{
    if (m_peer_map.count(bitmap.peer_id()))
        return;

    m_peer_map[bitmap.peer_id()] = peer_state_t(bitmap);

    peer_piece_bitmap_iterator_t it(bitmap, peer_piece_bitmap_t::pp_done);
    for (; !it.at_end(); ++it) {
        m_cumulative_piece_count[*it]++;
        m_peer_per_piece_list[*it].insert(bitmap.peer_id());
    }
}

void most_rarest_piece_pick_strategy_t::on_peer_removed(
    const peer_piece_bitmap_t &bitmap)
{
    if (!m_peer_map.count(bitmap.peer_id()))
        return;

    peer_piece_bitmap_iterator_t it(bitmap, peer_piece_bitmap_t::pp_done);
    for (; !it.at_end(); ++it) {
        m_cumulative_piece_count[*it]--;
        m_peer_per_piece_list[*it].erase(bitmap.peer_id());
    }

    m_peer_map.erase(bitmap.peer_id());
}

void most_rarest_piece_pick_strategy_t::on_piece_part_aborted(
        const piece_part_request_t &req)
{
    assert(m_peer_map.count(req.m_peer_id) > 0);

    m_mybitmap.set_piece_part(req.m_piece_index, req.m_piece_part_index,
        peer_piece_bitmap_t::pp_none);
    m_peer_map[req.m_peer_id].m_pending_count--;
}

void most_rarest_piece_pick_strategy_t::on_piece_part_requested(
        const piece_part_request_t &req)
{
    assert(m_peer_map.count(req.m_peer_id) > 0);
    m_mybitmap.set_piece_part(req.m_piece_index, req.m_piece_part_index,
        peer_piece_bitmap_t::pp_requested);
    m_peer_map[req.m_peer_id].m_pending_count++;
}

void most_rarest_piece_pick_strategy_t::on_piece_part_downloaded(
        const piece_part_request_t &req)
{
    assert(m_peer_map.count(req.m_peer_id) > 0);
    m_mybitmap.set_piece_part(req.m_piece_index, req.m_piece_part_index,
        peer_piece_bitmap_t::pp_downloaded);
}

void most_rarest_piece_pick_strategy_t::on_piece_validation_done(
    size_type piece_index)
{
    m_mybitmap.set_piece(piece_index);
}

void most_rarest_piece_pick_strategy_t::on_piece_validation_failed(
    size_type piece_index)
{
    m_mybitmap.unset_piece(piece_index);
}

}
