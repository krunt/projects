#ifndef PIECE_PART_REQUEST_DEF_
#define PIECE_PART_REQUEST_DEF_

namespace btorrent {

    struct piece_part_request_t {
        piece_part_request_t(const std::string &peer_id,
            size_type piece_index, size_type piece_part_index)
            : m_peer_id(peer_id),
              m_piece_index(piece_index),
              m_piece_part_index(piece_part_index)
        {}

        std::string m_peer_id;
        size_type m_piece_index;
        size_type m_piece_part_index;
    };

}

#endif /* PIECE_PART_REQUEST_DEF_ */
