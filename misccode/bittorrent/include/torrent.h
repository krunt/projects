#ifndef TORRENT_DEF_
#define TORRENT_DEF_

#include <include/common.h>

namespace btorrent {

class torrent {
public:
    void add_peer(const std::string &host, int port);
};

}


#endif /* TORRENT_DEF_ */
