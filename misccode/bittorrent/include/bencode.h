#ifndef BENCODE_DEF_
#define BENCODE_DEF_

#include <string>
#include <include/value.h>

namespace btorrent {

value_t bdecode(const std::string &str);
std::string bencode(const value_t &v);

}

#endif //BENCODE_DEF_
