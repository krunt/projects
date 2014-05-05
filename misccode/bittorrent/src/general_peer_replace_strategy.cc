#include <include/general_peer_replace_strategy.h>

namespace btorrent {
    
general_peer_replace_strategy_t::general_peer_replace_strategy_t(
    const peer_piece_bitmap_t &bitmap)
        : m_mybitmap(bitmap)
{
    m_peer_replacement_threshold = gsettings()->m_peer_replacement_threshold;
    m_peer_per_piece_list.resize(m_mybitmap.piece_count());
}

void general_peer_replace_strategy_t::on_piece_validation_done(
        size_type piece_index) 
{
    m_mybitmap.set_piece(piece_index);
    peer_list_per_piece_t &s = m_peer_per_piece_list[piece_index];
    for (peer_list_per_piece_t::const_iterator it = s.begin();
            it != s.end(); ++it) 
    {
        (*it)->m_piece_count_notmine--;
    }
}

void general_peer_replace_strategy_t::on_active_peer_added(
        const peer_piece_bitmap_t &bitmap)
{
    m_all_peers[bitmap.peer_id()] = peer_state_t(bitmap);

    ppeer_state_t p = &m_all_peers[bitmap.peer_id()];
    m_active_peers[bitmap.peer_id()] = p;

    peer_piece_bitmap_iterator_t it(bitmap, 1);
    for (; !it.at_end() && *it < m_mybitmap.piece_count(); ++it) {
        m_peer_per_piece_list[*it].insert(p);

        p->m_piece_count++;
        if (!m_mybitmap.has_piece(*it)) {
            p->m_piece_count_notmine++;
        }
    }
}

void general_peer_replace_strategy_t::on_inactive_peer_added(
        const peer_piece_bitmap_t &bitmap)
{
    m_all_peers[bitmap.peer_id()] = peer_state_t(bitmap);

    ppeer_state_t p = &m_all_peers[bitmap.peer_id()];
    m_inactive_peers[bitmap.peer_id()] = p;

    peer_piece_bitmap_iterator_t it(bitmap, 1);
    for (; !it.at_end() && *it < m_mybitmap.piece_count(); ++it) {
        m_peer_per_piece_list[*it].insert(p);

        p->m_piece_count++;
        if (!m_mybitmap.has_piece(*it)) {
            p->m_piece_count_notmine++;
        }
    }
}

void general_peer_replace_strategy_t::on_active_peer_removed(
        const peer_piece_bitmap_t &bitmap)
{
    assert(m_all_peers.count(bitmap.peer_id()) > 0);

    ppeer_state_t p = &m_all_peers[bitmap.peer_id()];
    m_active_peers.erase(bitmap.peer_id());

    peer_piece_bitmap_iterator_t it(bitmap, 1);
    for (; !it.at_end() && *it < m_mybitmap.piece_count(); ++it) {
        m_peer_per_piece_list[*it].erase(p);
    }
}

void general_peer_replace_strategy_t::on_inactive_peer_removed(
        const peer_piece_bitmap_t &bitmap)
{
    assert(m_all_peers.count(bitmap.peer_id()) > 0);

    ppeer_state_t p = &m_all_peers[bitmap.peer_id()];
    m_inactive_peers.erase(bitmap.peer_id());

    peer_piece_bitmap_iterator_t it(bitmap, 1);
    for (; !it.at_end() && *it < m_mybitmap.piece_count(); ++it) {
        m_peer_per_piece_list[*it].erase(p);
    }
}

general_peer_replace_strategy_t::ppeer_state_t 
general_peer_replace_strategy_t::get_max_item_from_list(
    const general_peer_replace_strategy_t::peer_map_type_t &mp) 
{
    ppeer_state_t result = NULL;
    general_peer_replace_strategy_t::peer_map_type_t::const_iterator 
        it = mp.begin();
    for (; it != mp.end(); ++it) {
        if (!result || result->m_piece_count_notmine 
                < (*it).second->m_piece_count_notmine)
        { result = (*it).second; }
    }
    return result;
}

general_peer_replace_strategy_t::ppeer_state_t 
general_peer_replace_strategy_t::get_min_item_from_list(
    const general_peer_replace_strategy_t::peer_map_type_t &mp) 
{
    ppeer_state_t result = NULL;
    general_peer_replace_strategy_t::peer_map_type_t::const_iterator 
        it = mp.begin();
    for (; it != mp.end(); ++it) {
        if (!result || result->m_piece_count_notmine 
                > (*it).second->m_piece_count_notmine)
        { result = (*it).second; }
    }
    return result;
}

void general_peer_replace_strategy_t::get_peer_replacements(
        std::vector<peer_replacement_t> &replacements)
{
    ppeer_state_t max_inactive = get_max_item_from_list(m_inactive_peers);
    ppeer_state_t min_active = get_min_item_from_list(m_active_peers);

    if (!max_inactive || !min_active) {
        return;
    }

    if (max_inactive->m_piece_count_notmine > min_active->m_piece_count_notmine
        && max_inactive->m_piece_count_notmine
        - min_active->m_piece_count_notmine > m_peer_replacement_threshold)
    {
        replacements.push_back(peer_replacement_t(
            max_inactive->peer_id(), min_active->peer_id()));
    }
}

bool general_peer_replace_strategy_t::get_peer_to_add(std::string &peer_id) {
    ppeer_state_t max_inactive = get_max_item_from_list(m_inactive_peers);

    if (max_inactive) {
        peer_id = max_inactive->peer_id();
        return true;
    }
    return false;
}

void general_peer_replace_strategy_t::get_peers_to_remove(
        std::vector<std::string> &peer_ids)
{
    peer_map_type_t::const_iterator it = m_active_peers.begin();
    for (; it != m_active_peers.end(); ++it) {
        if (!(*it).second->m_piece_count_notmine) {
            peer_ids.push_back((*it).first);
        }
    }
}

}

