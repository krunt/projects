#include <iostream>

#include <include/bencode.h>
#include <include/utils.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: <path to torrent-file>" << std::endl;
        return 1;
    }

    btorrent::value_t v = btorrent::bdecode(btorrent::bloat_file(argv[1]));
    std::cout << btorrent::serialize_to_json(v) << std::endl;

    return 0;
}
