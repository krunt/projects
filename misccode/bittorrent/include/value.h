#ifndef VALUE_DEF_
#define VALUE_DEF_

#include <include/types.h>

#include <string>
#include <vector>
#include <map>

#include <stdexcept>

namespace btorrent {

class bad_value_cast_exception: public std::exception {
};

class value_t {
public:
    typedef std::map<std::string, value_t> dictionary_type;
    typedef std::vector<value_t> list_type;
    typedef std::string string_type;
    typedef size_type integer_type;

    enum type_t { t_undef, t_dictionary, t_list, t_string, t_integer };

    value_t() : m_current_type(t_undef) {}
    value_t(const dictionary_type &d) : m_current_type(t_dictionary), m_dict(d) {}
    value_t(const list_type &lst) : m_current_type(t_list), m_list(lst) {}
    value_t(const string_type &str) : m_current_type(t_string), m_string(str) {}
    value_t(const integer_type &i) : m_current_type(t_integer), m_int(i) {}

    type_t get_type() const {
        return m_current_type;
    }

    bool exists(const std::string &key) const {
        if (to_dict().find(key) == to_dict().end()) {
            return false;
        }
        return true;
    }

    const value_t &operator[](int index) const {
        return to_list()[index];
    }

    value_t &operator[](int index) {
        return to_list()[index];
    }

    const value_t &operator[](const std::string &key) const {
        dictionary_type::const_iterator it;
        if ((it = to_dict().find(key)) == to_dict().end()) {
            throw std::runtime_error("no key found");
        }
        return (*it).second;
    }

    value_t &operator[](const std::string &key) {
        return to_dict()[key];
    }

    const dictionary_type &to_dict() const {
        check_conversion(t_dictionary);
        return m_dict;
    }

    const list_type &to_list() const {
        check_conversion(t_list);
        return m_list;
    }

    const string_type &to_string() const {
        check_conversion(t_string);
        return m_string;
    }

    const integer_type &to_int() const {
        check_conversion(t_integer);
        return m_int;
    }

    dictionary_type &to_dict() {
        check_conversion(t_dictionary);
        return m_dict;
    }

    list_type &to_list() {
        check_conversion(t_list);
        return m_list;
    }

    string_type &to_string() {
        check_conversion(t_string);
        return m_string;
    }

    integer_type &to_int() {
        check_conversion(t_integer);
        return m_int;
    }

private:
    void check_conversion(type_t destination_type) const {
        if (destination_type != m_current_type)
            throw bad_value_cast_exception();
    }

    type_t m_current_type;

    dictionary_type m_dict;
    list_type m_list;
    string_type m_string;
    integer_type m_int;
};

}

#endif //VALUE_DEF_
