
#include <include/bencode.h>
#include <boost/lexical_cast.hpp>

namespace btorrent {

class bad_encode_decode_exception: public std::runtime_error {
};

value_t bdecode(const std::string &str) {
    return value_t();
}



std::string bencode(const value_t &v) {
    std::string result;
    switch (v.get_type()) {
    case value_t::t_undef:     
        throw bad_encode_decode_exception("can't encode undef");
    break;
    case value_t::t_integer:
        result = 'i' + boost::lexical_cast<value_t::string_type>(v.to_int()) + 'e';
    break;
    case value_t::t_string:     
        result = boost::lexical_cast<value_t::string_type>(v.to_string().size()) 
            + ':' + v.to_string();
    break;
    case value_t::t_list: {
        result = "l";
        value_t::list_type &lst = v.to_list();
        for (int i = 0; i < lst.size(); ++i) {
            result.append(bencode(lst[i]));
        }
        result.append(1, 'e');
    break;
    }
    case value_t::t_dictionary: {
        result = "d";
        value_t::dictionary_type &mp = v.to_dict();
        for (value_t::dictionary_type b = mp.begin(); b != mp.end(); ++b) {
            const std::string &key = (*b).first;
            result += boost::lexical_cast<value_t::string_type>(key.size())
                + ':' + v.to_string();
            result.append(bencode((*b).second));
        }
        result.append(1, 'e');
    break;
    }};
    return result;
}

}
