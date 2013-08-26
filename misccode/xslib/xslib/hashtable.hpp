#ifndef XSLIB_HASHTABLE_HPP
#define XSLIB_HASHTABLE_HPP

#include <xslib/xscommon.hpp>
#include <boost/iterator/iterator_facade.hpp>

#include <utility>
#include <string>

namespace xslib {

class NotExists: std::exception {
};

class HashTable;
class HashTableTypeTraits {
public:
    typedef SV *     value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
};

static HV*
get_impl_shim(const HashTable &hsh);

class HashTableIterator
    : public boost::iterator_facade<
        HashTableIterator,
        std::pair<std::string, HashTableTypeTraits::value_type>,
        boost::bidirectional_traversal_tag,
        const std::pair<std::string, HashTableTypeTraits::value_type>
      >
{
public:
    typedef std::pair<std::string, HashTableTypeTraits::value_type> value_type;

public:
    HashTableIterator(const HashTable &hsh, bool position_to_end = false) 
        : hashtable(hsh)
    { 
        if (!position_to_end) { 
            reset(); 
        }
    }

private:
    friend class boost::iterator_core_access;

    void reset() {
        hv_iterinit(get_impl_shim(hashtable));
        current_ = hv_iternext(get_impl_shim(hashtable));
    }

    void increment() {
        current_ = hv_iternext(get_impl_shim(hashtable));
    }

    bool equal(const HashTableIterator &other) const {
        return current_ == other.current_;
    }

    const value_type dereference() const {
        I32 len;
        std::string key(hv_iterkey(current_, &len));
        SV *value = hv_iterval(get_impl_shim(hashtable), current_);
        return std::make_pair(key, value);
    }

private:
    const HashTable &hashtable;
    HE *current_;
};

class HashTable: public xscommon<HV, HashTable> {
public:
    typedef HashTableTypeTraits::value_type value_type;
    typedef HashTableTypeTraits::pointer pointer;
    typedef HashTableTypeTraits::const_pointer const_pointer;
    typedef HashTableTypeTraits::reference reference;
    typedef HashTableTypeTraits::const_reference const_reference;
    typedef HashTableTypeTraits::size_type size_type;
    typedef HashTableTypeTraits::difference_type difference_type;
    
public:
    HashTable(HV *hv)
        : xscommon<HV, HashTable>(hv, true)
    {}

    const value_type operator[](const std::string &key) const {
        if (!exists(key)) {
            throw NotExists();
        }
        return static_cast<const value_type>(
            *hv_fetch(impl(), key.c_str(), key.length(), 0));
    }

    int length() const { 
        return HvKEYS(impl()); 
    }

    void clear() {
        hv_clear(impl());
    }

    bool exists(const std::string &key) const {
        return hv_exists(impl(), key.c_str(), key.length());
    }

    bool operator==(const HashTable &other) const {
        return impl() == other.impl();
    }

    HashTableIterator begin() {
        return HashTableIterator(*this, false);
    }

    HashTableIterator end() {
        return HashTableIterator(*this, true);
    }

    bool typeok() const {
        return (SV*)impl() != &PL_sv_undef && SvTYPE(impl()) == SVt_PVHV;
    }
};

HV* get_impl_shim(const HashTable &hsh) {
    return hsh.impl();
}

}

#endif /* XSLIB_HASHTABLE_HPP */
