#ifndef UTILS_DEF_
#define UTILS_DEF_

namespace btorrent {

std::string serialize_to_json(const value_t &v);
std::string bloat_file(const std::string &full_path);

}

#endif //UTILS_DEF_
