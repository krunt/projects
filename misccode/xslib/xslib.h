#ifndef XSLIB_DEF_HPP
#define XSLIB_DEF_HPP

#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

#include <xslib/integer.hpp>
#include <xslib/function.hpp>
#include <xslib/reference.hpp>
#include <xslib/string.hpp>

#include <xslib/array.hpp>
#include <xslib/hashtable.hpp>

/* must be included last */
#include <xslib/return.hpp>

namespace xslib {

class Integer;
class Reference;
class String;
class Array;
class HashTable;

/* improve error reporting here */
class bad_cast: std::exception {
public:
    virtual const char *what() const throw() {
        return "bad typecast";
    }
};

template <typename T, typename V>
T typecast(V *sv) {
    T result(sv);

#ifdef XSLIB_TYPECHECKING
    if (!result.istypeok()) {
        throw bad_cast();
    }
#endif /* XSLIB_TYPECHECKING */

    return result;
}

template <typename T, typename V, typename U>
T typecast(V *sv, U arg) {
    T result(sv, arg);

#ifdef XSLIB_TYPECHECKING
    if (!result.istypeok()) {
        throw bad_cast();
    }
#endif /* XSLIB_TYPECHECKING */

    return result;
}

template <typename T>
void make_result_impl(SV **&sp, T &val) {
    return Return<T>()(sp, val);
}

#define make_result(val) make_result_impl(sp, val)

}

#endif /* XSLIB_DEF_HPP */
