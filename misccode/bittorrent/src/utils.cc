#include <include/value.h>
#include <boost/lexical_cast.hpp>

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

}
