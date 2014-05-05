#include <include/common.h>
#include <include/most_rarest_piece_pick_strategy.h>

namespace btorrent {

most_rarest_piece_pick_strategy_t::most_rarest_piece_pick_strategy_t(
    const peer_piece_bitmap_t &my_bitmap)
    : m_mybitmap(my_bitmap), 
      m_max_pending_requests_per_peer(gsettings()->m_max_pending_requests_per_peer)
{
    m_cumulative_piece_count.resize(m_mybitmap.piece_count(), 0);
    m_peer_per_piece_list.resize(m_mybitmap.piece_count());
}

DEFINE_METHOD(bool, most_rarest_piece_pick_strategy_t::get_next_part,
    piece_part_request_t &req)

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

        peer_piece_part_bitmap_iterator_t it(m_mybitmap,
            piece_index, peer_piece_bitmap_t::pp_none);
        for (; !it.at_end(); ++it) {
            for(peer_list_per_piece_t::const_iterator it1 = peer_list.begin();
                    it1 != peer_list.end(); ++it1) 
            {
                if ((*it1)->m_pending_count < m_max_pending_requests_per_peer) {

                    req.m_peer_id = (*it1)->m_bitmap.peer_id();
                    req.m_piece_index = piece_index;
                    req.m_piece_part_index = *it;

                    return true;

                } 
                
                /*
                else {
                    GLOG->debug("can't make request because %s got pending %d/%d",
                        (*it1)->m_bitmap.peer_id().c_str(),
                        (*it1)->m_pending_count,
                        m_max_pending_requests_per_peer);
                }
                */
            }
        }
    }

    /* none found */
    return false;
END_METHOD

DEFINE_METHOD(void, most_rarest_piece_pick_strategy_t::on_peer_added,
    const peer_piece_bitmap_t &bitmap)

    GLOG->debug("peer added %s with bitmap %s", 
            bitmap.peer_id().c_str(), bitmap.to_string().c_str());

    if (m_peer_map.count(bitmap.peer_id()))
        return;

    m_peer_map[bitmap.peer_id()] = peer_state_t(bitmap);

    peer_piece_bitmap_iterator_t it(bitmap, 1);
    for (; !it.at_end(); ++it) {
        if (*it >= m_peer_per_piece_list.size()) {
            break;
        }

        GLOG->debug("%s has %d piece", bitmap.peer_id().c_str(), *it);
        m_cumulative_piece_count[*it]++;
        m_peer_per_piece_list[*it].insert(&m_peer_map[bitmap.peer_id()]);
    }

END_METHOD

void most_rarest_piece_pick_strategy_t::on_peer_removed(
    const peer_piece_bitmap_t &bitmap)
{
    if (!m_peer_map.count(bitmap.peer_id()))
        return;

    peer_piece_bitmap_iterator_t it(bitmap, 1);
    for (; !it.at_end(); ++it) {
        if (*it >= m_peer_per_piece_list.size()) {
            break;
        }

        m_cumulative_piece_count[*it]--;
        m_peer_per_piece_list[*it].erase(&m_peer_map[bitmap.peer_id()]);
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

DEFINE_METHOD(void, most_rarest_piece_pick_strategy_t::on_piece_part_downloaded,
        const piece_part_request_t &req)

    GLOG->debug("piece=%d part=%d", req.m_piece_index, req.m_piece_part_index);

    assert(m_peer_map.count(req.m_peer_id) > 0);
    m_mybitmap.set_piece_part(req.m_piece_index, req.m_piece_part_index,
        peer_piece_bitmap_t::pp_downloaded);

    m_peer_map[req.m_peer_id].m_pending_count--;

END_METHOD

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
