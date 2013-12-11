#ifndef COMMON_DEF_
#define COMMON_DEF_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <include/types.h>
#include <include/hash.h>
#include <include/value.h>
#include <include/utils.h>
#include <include/torrent_info.h>
#include <include/bencode.h>
#include <include/logger.h>

/* forward declarations */
namespace btorrent {
class peer_t;
typedef peer_t *ppeer_t;

class torrent_t;
}

#include <include/peer_replacement.h>
#include <include/peer_piece_bitmap.h>
#include <include/piece_part_request.h>
#include <include/piece_pick_strategy.h>
#include <include/peer_replace_strategy.h>

#include <include/tracker_connection.h>

#include <include/settings.h>

#endif
