#include <include/peer.h>
#include <boost/bind.hpp>

namespace btorrent {

void peer_t::start_active() {
    assert(m_state == s_none);
    m_state = s_bitmap_active_pending;
    setup_callbacks();
    m_conn.start();
}

void peer_t::start_pending() {
    assert(m_state == s_none);
    m_state = s_bitmap_pending;
    setup_callbacks();
    m_conn.start();
}

void peer_t::make_request(const piece_part_request_t &request) {
    assert(m_state == s_active);
    assert(request.m_peer_id == m_peer_id);

    m_pending_requests.push_back(request);
    m_conn.send_request(request.m_piece_index, 
            request.m_piece_part_index * gsettings()->m_piece_part_size, 
            gsettings()->m_piece_part_size, true /* async = true */);
}

void peer_t::finish() {
    m_state = m_state == s_bitmap_done ? s_finishing_from_pending
        : s_finishing_from_active;
}

void peer_t::finish_with_replacement() {
    m_state = s_in_replacement;
}

void peer_t::setup_callbacks() {
    m_conn.setup_handshake_done_callback(
        boost::bind(&peer_t::on_handshake_done, this));
    m_conn.setup_connection_lost_callback(
        boost::bind(&peer_t::on_connection_lost, this));
    m_conn.setup_bitmap_received_callback(
        boost::bind(&peer_t::on_bitmap_received, this, _1));
    m_conn.setup_piece_part_received_callback(
        boost::bind(&peer_t::on_piece_part_received, this, _1, _2, _3));
    m_conn.setup_piece_part_requested_callback(
        boost::bind(&peer_t::on_piece_part_requested, this, _1, _2));
}

void peer_t::on_handshake_done() {
    m_conn.send_unchoke();
    m_conn.send_interested();
}

void peer_t::on_connection_lost() {
    /* clear pending requests statuses */
    for (int i = 0; i < m_pending_requests.size(); ++i) {
        m_torrent.on_aborted_request(this, m_pending_requests[i]);
    }

    m_pending_requests.clear();
    m_torrent.on_connection_lost(this);
    m_state = s_none;
}

void peer_t::on_bitmap_received(const std::vector<u8> &bitmap) {
    if (m_state == s_bitmap_pending) {
        m_state = s_bitmap_done;
    } else if (m_state == s_bitmap_active_pending) {
        m_state = s_active;
    }
    m_bitmap = peer_piece_bitmap_t(m_peer_id, bitmap,
       m_torrent.get_torrent_info().m_piece_size / gsettings()->m_piece_part_size);
    m_torrent.on_bitmap_received(this, bitmap);
}

DEFINE_METHOD(void, peer_t::remove_from_pending, size_type piece_index,
    size_type piece_part_index) 

    std::vector<piece_part_request_t> new_requests;
    for (int i = 0; i < m_pending_requests.size(); ++i) {
        if (m_pending_requests[i].m_piece_index != piece_index
            || m_pending_requests[i].m_piece_part_index != piece_part_index)
        {
            new_requests.push_back(m_pending_requests[i]);
        }
    }

    GLOG->debug("got %d/%d", new_requests.size(), m_pending_requests.size());
    m_pending_requests.swap(new_requests);

END_METHOD


DEFINE_METHOD(void, peer_t::on_piece_part_received, size_type piece_index,
    size_type piece_part_index, const std::vector<u8> &data)

    remove_from_pending(piece_index, piece_part_index);

    /* we are done */
    if ((is_finishing() || m_state == s_in_replacement)
        && m_pending_requests.empty())
    {
        on_connection_lost();
    }

    m_torrent.on_piece_part_received(this, piece_index, piece_part_index, data);

END_METHOD

DEFINE_METHOD(void, peer_t::on_piece_part_requested, size_type piece_index,
    size_type piece_part_index)

    std::vector<u8> data;

    m_torrent.on_piece_part_requested(this, piece_index, piece_part_index, data);

END_METHOD

DEFINE_METHOD(void, peer_t::on_piece_part_requested_done, size_type piece_index,
    size_type piece_part_index, const std::vector<u8> &data)

    if (!data.empty()) {
        m_conn.send_piece(piece_index, piece_part_index 
            * gsettings()->m_piece_part_size, data, true);
    }

END_METHOD

DEFINE_METHOD(void, peer_t::on_piece_validation_done, int piece_index)
    GLOG->debug("sending have packet piece=%d", piece_index);
    m_conn.send_have(piece_index, true);
END_METHOD

}

