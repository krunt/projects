#ifndef HASH_DEF_
#define HASH_DEF_

#include <string>

namespace btorrent {

class sha1_hash_t {
public:
    sha1_hash_t()
    {}

    sha1_hash_t(const std::string &val)
        : m_val(val)
    {}

    void init() {}
    void update(const std::string &s) {}
    void finalize() {}

private
    std::string m_val;
};

}

#endif //HASH_DEF_
