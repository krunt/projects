#include <include/value.h>

namespace btorrent {

namespace detail {
static std::string serialize_to_json(const value_t &v, int indent) {
    std::string result;
    switch (v.get_type()) {
    case value_t::t_undef:     
        throw bad_encode_decode_exception("can't serialize undef to json");
    break;
    case value_t::t_integer:
        result = boost::lexical_cast<value_t::string_type>(v.to_int());
    break;
    case value_t::t_string:     
        result = "\"" 
            + boost::lexical_cast<value_t::string_type>(v.to_string().size()) 
            + "\"";
    break;
    case value_t::t_list: {
        const value_t::list_type &lst = v.to_list();
        result += std::string(indent, ' ') + "[\n";
        for (int i = 0; i < lst.size(); ++i) {
            result.append(detail::serialize_to_json(lst[i], 0));
            if (i != lst.size()) { 
                result += ",\n";
            }
        }
        result += std::string(indent, ' ') + "]\n";
    break;
    }
    case value_t::t_dictionary: {
        const value_t::dictionary_type &mp = v.to_dict();
        result += std::string(indent, ' ') + "{\n";
        for (value_t::dictionary_type::const_iterator 
                b = mp.begin(); b != mp.end();) 
        {
            const std::string &key = (*b).first;
            result += "\"" + key + "\":";
            result.append(detail::serialize_to_json((*b).second));

            if (++b != mp.end()) {
                result += ",\n";
            }
        }
        result += std::string(indent, ' ') + "}\n";
    break;
    }};
    return result;
}
}

std::string serialize_to_json(const value_t &v) {
    return detail::serialize_to_json(v, 0);
}

}
