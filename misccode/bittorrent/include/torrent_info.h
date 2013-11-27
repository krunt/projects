#ifndef TORRENT_INFO_DEF_
#define TORRENT_INFO_DEF_

#include <include/common.h>

namespace btorrent {

struct torrent_info_t {
    std::string m_announce_url;
    std::vector<std::string> m_announce_list;

    sha1_hash_t m_info_hash;

    size_type m_piece_size;
    std::vector<sha1_hash_t> m_piece_hashes;

    struct file_t { 
        file_t(const std::string &path, size_type sz)
            : m_path(path), m_size(sz)
        {}

        std::string m_path;
        size_type m_size;
    };

    std::vector<file_t> m_files;
};

torrent_info_t construct_torrent_info(const std::string &filename);

}

#endif //TORRENT_INFO_DEF_
