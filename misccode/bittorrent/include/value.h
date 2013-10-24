#include <include/types.h>

#include <vector>

namespace btorrent {

class bad_value_cast_exception: public std::exception {
};

class value_t {
public:
    typedef std::map<std::string, value> dictionary_type;
    typedef std::vector<value> list_type;
    typedef std::string string_type;
    typedef size_type integer_type;

    enum type_t { t_undef, t_dictionary, t_list, t_string, t_integer };

    value_t() : m_current_type(t_undef) {}
    value_t(const dictionary_type &d) : m_current_type(t_dictionary), m_dict(d) {}
    value_t(const list_type &lst) : m_current_type(t_list), m_list(lst) {}
    value_t(const string_type &str) : m_current_type(t_string), m_string(str) {}
    value_t(const integer_type &i) : m_current_type(t_string), m_integer(i) {}

    dictionary_type &to_dict() const {
        check_conversion(t_dictionary);
        return m_dict;
    }

    list_type &to_list() const {
        check_conversion(t_list);
        return m_list;
    }

    string_type &to_string() const {
        check_conversion(t_string);
        return m_string;
    }

    integer_type &to_int() const {
        check_conversion(t_integer);
        return m_int;
    }

private:
    void check_conversion(type_t destination_type) {
        if (destination_type != m_current_type)
            throw detail::bad_value_cast_exception();
    }

    type_t m_current_type;

    dictionary_type m_dict;
    list_type m_list;
    string_type m_string;
    integer_type m_int;
};

}
