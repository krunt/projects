#include <include/utils.h>
#include <include/value.h>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include <fstream>
#include <iterator>
namespace btorrent {

namespace detail {
static std::string serialize_to_json(const value_t &v, int indent) {
    std::string result;
    switch (v.get_type()) {
    case value_t::t_integer:
        result = std::string(indent, ' ') 
            + boost::lexical_cast<value_t::string_type>(v.to_int());
    break;
    case value_t::t_string:     
        result = std::string(indent, ' ') + "\"" 
            + v.to_string()
            + "\"";
    break;
    case value_t::t_list: {
        const value_t::list_type &lst = v.to_list();
        result += "\n" + std::string(indent, ' ') + "[\n";
        for (int i = 0; i < lst.size(); ++i) {
            result.append(detail::serialize_to_json(lst[i], indent + 2));
            if (i + 1 < lst.size()) { 
                result += "\n";
            }
        }
        result += "\n" + std::string(indent, ' ') + "]\n";
    break;
    }
    case value_t::t_dictionary: {
        const value_t::dictionary_type &mp = v.to_dict();
        result += "\n" + std::string(indent, ' ') + "{\n";
        for (value_t::dictionary_type::const_iterator 
                b = mp.begin(); b != mp.end();) 
        {
            const std::string &key = (*b).first;
            result += std::string(indent + 2, ' ') + "\"" + key + "\":";
            result.append(detail::serialize_to_json((*b).second, indent + 2));

            if (++b != mp.end()) {
                result += "\n";
            }
        }
        result += "\n" + std::string(indent, ' ') + "}\n";
    break;
    }};
    return result;
}
}

std::string serialize_to_json(const value_t &v) {
    return detail::serialize_to_json(v, 0);
}

std::string bloat_file(const std::string &full_path) {
    std::string contents;
    std::fstream fs(full_path.c_str(), std::ios_base::in);

    fs.unsetf(std::ios::skipws);
    std::copy(std::istream_iterator<char>(fs), std::istream_iterator<char>(),
        std::back_inserter(contents));
    return contents;
}

/* TODO */
std::string hex_encode(const std::string &s) { 
    return std::string(20, '\0'); 
}
std::string hex_decode(const std::string &s) { 
    return std::string(20, '\0'); 
}

void generate_random(char *ptr, size_type sz) {
    for (int i = 0; i < sz; ++i) {
        ptr[i] = rand() & 0xFF;
    }
}

url_t::url_t(const std::string &url) {
    const boost::regex url_re("^(http|udp)://([^/:]+)(?::([0-9]+))?(/.*)?$"); 
    boost::cmatch what;
    if (boost::regex_match(url.c_str(), what, url_re)) {
        m_scheme = what[1];
        m_host = what[2];
        if (what[3].matched) m_port = boost::lexical_cast<int>(what[3]);
        else if (m_scheme == "http") m_port = 80;
        if (what[4].matched) m_path = what[4];
    }
}

}
