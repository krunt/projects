#ifndef HASH_DEF_
#define HASH_DEF_

#include <include/common.h>

namespace btorrent {

class sha1_hash_t {
public:
    sha1_hash_t()
    {}

    sha1_hash_t(const std::string &digest);

    void init();
    void update(const char *data, size_type len);
    void update(const std::string &s);
    void finalize();

    std::string get_digest() const;

private:
    typedef struct AVSHA {
        u64 count;       ///< number of bytes in buffer
        u8  buffer[64];  ///< 512-bit buffer of input values used in hash updating
        u32 state[8];    ///< current hash value
        u8  digest[20];
    } AVSHA;

    AVSHA m_ctx;
};

}

#endif //HASH_DEF_
