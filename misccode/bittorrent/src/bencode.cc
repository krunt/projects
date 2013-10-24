
#include <include/bencode.h>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <algorithm>
#include <stdexcept>

namespace btorrent {

class bad_encode_decode_exception: public std::runtime_error {
public:
    bad_encode_decode_exception(const std::string &str)
        : std::runtime_error(str) {}
};

namespace detail {

    size_type parse_integer(const std::string &str, int &offs) {
        size_type num_value = 0;
        bool negative = false;

        while (offs < str.size() && str[offs] == '-') {
            negative = !negative;
            offs++;
        }

        while (offs < str.size() && isdigit(str[offs])) {
            num_value = num_value * 10 + (str[offs] - '0');
            offs++;
        }

        if (offs >= str.size() || str[offs] != 'e') {
            throw bad_encode_decode_exception("bad integer format, expected `e'");
        }

        if (negative) { num_value *= -1; }

        offs++;
        return num_value;
    }

    std::string parse_string(const std::string &str, int &offs) {
        size_type string_length = 0;
        while (offs < str.size() && isdigit(str[offs])) {
            string_length = string_length * 10 + (str[offs] - '0');
            offs++;
        }

        if (offs >= str.size() || str[offs] != ':') {
            throw bad_encode_decode_exception("bad integer format, expected `:'");
        }

        ++offs;

        if (str.size() - offs < string_length) {
            throw bad_encode_decode_exception("shortened string");
        }

        offs += string_length;

        return str.substr(offs - string_length, string_length);
    }

    value_t bdecode_internal(const std::string &str, int &offs) {
        switch (str[offs]) {
    
        /* number */
        case 'i': {
            offs++;
            return value_t(parse_integer(str, offs)); 
        }
            
        /* string */
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
        case '8': case '9': {
            return value_t(parse_string(str, offs));
        }
    
        case 'l': {
            value_t::list_type lst;

            offs++;

            do {
                lst.push_back(bdecode_internal(str, offs));
            } while (offs < str.size() && str[offs] != 'e');

            if (offs >= str.size() || str[offs] != 'e') {
                throw bad_encode_decode_exception("unexpected end of list");
            }

            offs++;
            return value_t(lst);
        }
    
        case 'd': {
            value_t::dictionary_type dict;

            offs++;

            do {
                value_t k = bdecode_internal(str, offs);
                value_t v = bdecode_internal(str, offs);
                dict[k.to_string()] = v; 
            } while (offs < str.size() && str[offs] != 'e');

            if (offs >= str.size() || str[offs] != 'e') {
                throw bad_encode_decode_exception("unexpected end of dict");
            }

            offs++;
            return value_t(dict);
        }

        default:
            throw bad_encode_decode_exception("invalid token");
        };
    }
}

value_t bdecode(const std::string &str) {
    int offset = 0;
    value_t result = detail::bdecode_internal(str, offset);
    if (offset != str.size()) {
        throw bad_encode_decode_exception(
                "invalid bdecode, unexpected end-of-file");
    }
    return result;
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
        const value_t::list_type &lst = v.to_list();
        for (int i = 0; i < lst.size(); ++i) {
            result.append(bencode(lst[i]));
        }
        result.append(1, 'e');
    break;
    }
    case value_t::t_dictionary: {
        result = "d";
        const value_t::dictionary_type &mp = v.to_dict();
        for (value_t::dictionary_type::const_iterator 
                b = mp.begin(); b != mp.end(); ++b) 
        {
            const std::string &key = (*b).first;
            result += boost::lexical_cast<value_t::string_type>(key.size())
                + ':' + key;
            result.append(bencode((*b).second));
        }
        result.append(1, 'e');
    break;
    }};
    return result;
}

}
