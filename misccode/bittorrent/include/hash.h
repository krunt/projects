#ifndef HASH_DEF_
#define HASH_DEF_

class sha1_hash_t {
public:
    sha1_hash_t(const std::string &val)
        : m_val(val)
    {}

private
    std::string m_val;
};

#endif //HASH_DEF_
