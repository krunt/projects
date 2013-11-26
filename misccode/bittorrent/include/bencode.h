#ifndef BENCODE_DEF_
#define BENCODE_DEF_

#include <string>
#include <include/value.h>

namespace btorrent {

class bad_encode_decode_exception: public std::runtime_error {
public:
    bad_encode_decode_exception(const std::string &str)
        : std::runtime_error(str) {}
};


value_t bdecode(const std::string &str);
std::string bencode(const value_t &v);

}

#endif //BENCODE_DEF_
