#ifndef TORRENT_INFO_DEF_
#define TORRENT_INFO_DEF_

struct torrent_info_t {
    value_t m_announce_url;
    
    size_type m_piece_length;
    std::vector<sha1_hash_t> m_piece_hashes;
    std::vector<std::string> m_files;
};

#endif //TORRENT_INFO_DEF_
