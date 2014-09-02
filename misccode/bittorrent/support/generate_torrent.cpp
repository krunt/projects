#include <fstream>
#include <iostream>
#include <stdexcept>

#include <boost/filesystem.hpp>

#include <include/common.h>

const int PIECE_SIZE = 4 * 1024 * 1024;

using btorrent::value_t;

static int get_file_size(const std::string &filename) {
    struct stat st;
    stat(filename.c_str(), &st);
    return st.st_size;
}

void calculate_piece_hashes(std::string &hashes, 
        const std::string &filename) 
{
    int n, to_read;
    std::string buffer;
    std::fstream in_stream(filename.c_str(), 
            std::ios_base::in | std::ios_base::binary);

    hashes.clear();

    /* calculate hashes and store them to file */
    while (1) {
        btorrent::sha1_hash_t hash;
        hash.init();

        buffer.resize(PIECE_SIZE, 0);

        const char *p = buffer.data();
        to_read = PIECE_SIZE;
        in_stream.read((char *)p, to_read);
        if (in_stream) {
            to_read = 0;
        } else {
            to_read = PIECE_SIZE - in_stream.gcount();
        }

        if (to_read) {
            memset((char *)p + in_stream.gcount(), 0, to_read);
        }

        hash.update(buffer);
        hash.finalize();

        hashes += hash.get_digest();

        if (to_read) {
            break;
        }
    }
}

void create_torrent(const std::string &filename, const std::string &outfilename) {
    value_t::dictionary_type root, info;

    info["name"] = value_t(boost::filesystem::path(filename).filename());
    info["length"] = value_t(get_file_size(filename));
    info["piece length"] = value_t(PIECE_SIZE);

    std::string piece_hashes;
    calculate_piece_hashes(piece_hashes, filename);
    info["pieces"] = value_t(piece_hashes);

    root["announce"] = value_t("http://localhost:13101/x");
    root["info"] = value_t(info);

    value_t::list_type announce_list, udp1, http1;

    udp1.push_back(value_t("udp://localhost:13101/x"));
    http1.push_back(value_t("http://localhost:13101/x"));

    announce_list.push_back(udp1);
    announce_list.push_back(http1);

    root["announce-list"] = announce_list;

    std::fstream out_stream(outfilename.c_str(), 
            std::ios_base::out | std::ios_base::binary);
    out_stream << btorrent::bencode(value_t(root));
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: <path to file to share> <out-torrent-file>" 
            << std::endl;
        return 1;
    }

    create_torrent(argv[1], argv[2]);

    return 0;
}

