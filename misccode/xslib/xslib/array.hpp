#ifndef XSLIB_ARRAY_HPP
#define XSLIB_ARRAY_HPP

#include <xslib/xscommon.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace xslib {

class Array;
class ArrayTypeTraits {
public:
    typedef SV *     value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
};
static const ArrayTypeTraits::value_type index_shim(const Array &array, int index);

class ArrayIterator
    : public boost::iterator_facade<
        ArrayIterator,
        ArrayTypeTraits::value_type,
        boost::random_access_traversal_tag,
        const ArrayTypeTraits::value_type
      >
{
public:
    ArrayIterator(const Array &arr, int itbegin, int itend)
        : array_(arr), start(itbegin), end(itend)
    {}

private:
    friend class boost::iterator_core_access;

    void increment() {
        start++;
    }

    bool equal(const ArrayIterator &other) const {
        return start == other.start && end == other.end;
    }

    const ArrayTypeTraits::value_type dereference() const {
        return static_cast<const ArrayTypeTraits::value_type>(
                index_shim(array_, start));

    }

private:
    const Array &array_;
    int start;
    int end;
};

class Array: public xscommon<AV, Array> {
public:
    typedef ArrayTypeTraits::value_type value_type;
    typedef ArrayTypeTraits::pointer pointer;
    typedef ArrayTypeTraits::const_pointer const_pointer;
    typedef ArrayTypeTraits::reference reference;
    typedef ArrayTypeTraits::const_reference const_reference;
    typedef ArrayTypeTraits::size_type size_type;
    typedef ArrayTypeTraits::difference_type difference_type;
    
public:
    Array(AV *av)
        : xscommon<AV, Array>(av, true)
    {}

    value_type shift() {
        return static_cast<value_type>(av_shift(impl()));
    }

    void unshift(value_type val) {
        av_unshift(impl(), 1);
        av_store(impl(), 0, val);
    }

    const value_type operator[](int index) const {
        return static_cast<value_type>(*av_fetch(impl(), index, 0));
    }

    void push(value_type val) {
        av_push(impl(), val);
    }

    void pop() {
        av_pop(impl());
    }

    void clear() {
        av_clear(impl());
    }

    int length() const {
        return av_len(impl()) + 1;
    }

    bool operator==(const Array &other) const {
        return impl() == other.impl();
    }

    ArrayIterator begin() {
        return ArrayIterator(*this, 0, length());
    }

    ArrayIterator end() {
        return ArrayIterator(*this, length(), length());
    }

    bool typeok() const {
        return (SV*)impl() != &PL_sv_undef && SvTYPE(impl()) == SVt_PVAV;
    }
};

const ArrayTypeTraits::value_type index_shim(const Array &array, int index) {
    return array[index];
}

}

#endif /* XSLIB_ARRAY_HPP */
