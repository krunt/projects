#include <include/torrent_info.h>
#include <include/value.h>

#include <string>

namespace btorrent {

torrent_info_t construct_torrent_info(const std::string &filename) {
    btorrent::value_t v = btorrent::bdecode(btorrent::bloat_file(filename));

    torrent_info_t r;
    r.m_announce_url = v["announce"].to_string();
    if (v.exists("announce-list")) {
        const value_t::list_type &lst = v["announce-list"].to_list();
        for (int i = 0; i < lst.size(); ++i) {
            r.m_announce_list.push_back(lst[i][0].to_string());
        }
    }

    /* info-hash */
    {
        const std::string info_str = bencode(v["info"]);
        r.m_info_hash.init();
        r.m_info_hash.update(info_str);
        r.r.m_info_hash.finalize();
    }

    r.m_piece_length = v["info"]["piece_length"].to_int();
    const value_t::string_type &pieces = v["info"]["pieces"].to_string();
    for (int i = 0; i < pieces.size() / 20; ++i) {
        r.m_piece_hashes.push_back(sha1_hash_t(
            pieces.substr(i * 20, 20)));
    }

    /* multifile case */
    if (v["info"].exists("files")) {
        const value_t::list_type &lst = v["info"]["files"].to_list();
        for (int i = 0; i < lst.size(); ++i) {
            r.m_files.push_back(torrent_info_t::file_t(
                        lst[i]["path"][0].to_string(),
                        lst[i]["length"].to_int()));
        }
    } else {
        r.m_files.push_back(torrent_info_t::file_t(
            v["info"]["name"].to_string(), v["info"]["length"].to_int()));
    }

    return r;
}

}
