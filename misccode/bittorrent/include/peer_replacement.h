#ifndef PEER_REPLACEMENT_DEF_
#define PEER_REPLACEMENT_DEF_

namespace btorrent {

struct peer_replacement_t {
    peer_replacement_t(const std::string &from, const std::string &to)
        : m_from_peer_id(from), m_to_peer_id(to)
    {}

    bool empty() const { return m_from_peer_id.empty() 
        && m_to_peer_id.empty(); }

    std::string m_from_peer_id;
    std::string m_to_peer_id;
};

}

#endif /* PEER_REPLACEMENT_DEF_ */
