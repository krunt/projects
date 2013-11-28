#ifndef PIECE_PART_REQUEST_DEF_
#define PIECE_PART_REQUEST_DEF_

namespace btorrent {

    struct piece_part_request_t {
        std::string m_peer_id;
        size_type m_piece_index;
        size_type m_piece_part_index;
    };

}

#endif /* PIECE_PART_REQUEST_DEF_ */
