#ifndef UTILS_DEF_
#define UTILS_DEF_

#include <include/common.h>

namespace btorrent {

std::string serialize_to_json(const value_t &v);
std::string bloat_file(const std::string &full_path);

std::string hex_decode(const std::string &s);
std::string hex_encode(const std::string &s);

void generate_random(char *ptr, size_type sz);

class url_t {
public:
    url_t(const std::string &url);

    std::string get_scheme() const { return m_scheme; }
    std::string get_host() const { return m_host; }
    int get_port() const { return m_port; }
    std::string get_path() const { return m_path; }
    
private:
    std::string m_scheme;
    std::string m_host;
    int m_port;
    std::string m_path;
};

}

#endif //UTILS_DEF_
