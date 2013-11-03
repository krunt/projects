#ifndef UTILS_DEF_
#define UTILS_DEF_

namespace btorrent {

std::string serialize_to_json(const value_t &v);
std::string bloat_file(const std::string &full_path);

std::string hex_decode(const std::string &s);
std::string hex_encode(const std::string &s);

void generate_random(char *ptr, size_type sz);

}

#endif //UTILS_DEF_
