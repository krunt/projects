#ifndef UTILS_DEF_
#define UTILS_DEF_

#include <include/common.h>
#include <boost/asio.hpp>

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

namespace utils {
    std::string ipv4_to_string(u32 ip);
    std::string escape_http_string(const std::string &str);

    template <typename SocketType>
    inline void read_available_from_socket(SocketType &s, 
            std::vector<u8> &buffer, size_t max_size = -1) 
    {
        if (s.available()) {
            size_t bytes_to_read = std::min(s.available(), max_size);
            buffer.resize(buffer.size() + bytes_to_read);
            s.receive(boost::asio::buffer(
                reinterpret_cast<u8*>(&buffer[0]) 
                + buffer.size() - bytes_to_read, bytes_to_read));
        }
    }
}

}

#endif //UTILS_DEF_
