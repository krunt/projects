#include <iostream>

#include <include/common.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: <path to torrent-file>" << std::endl;
        return 1;
    }

    btorrent::torrent_info_t t = btorrent::construct_torrent_info(argv[1]);

    return 0;
}
