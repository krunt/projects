#ifndef XSLIB_COMMON_HPP
#define XSLIB_COMMON_HPP

#include <xslib/common.hpp>

namespace xslib {

template <typename T, typename Derived>
class xscommon {
public:
    xscommon(T *sv, bool owns_scalar = true)
        : impl_(sv)
    { 
        if (owns_scalar) { 
            SvREFCNT_inc(impl_); 
        } 
    }

    xscommon(const xscommon<T, Derived> &other)
        : impl_(other.impl_)
    { SvREFCNT_inc(impl_); }

    xscommon<T, Derived> &operator=(const xscommon<T, Derived> &other) {
        xscommon<T, Derived> tmp(other);
        tmp.swap(*this);
        return *this;
    }

    ~xscommon() {
        if (impl_)
            SvREFCNT_dec(impl_);
        impl_ = NULL;
    }

    T *dispose() {
        T *res = impl_;
        impl_ = NULL;
        return res;
    }

    void swap(xscommon<T, Derived> &other) {
        std::swap(impl_, other.impl_);
    }

    T *impl() const {
        return impl_;
    }

    bool typeok() const { 
        return false; 
    }

    bool istypeok() const {
        if (!impl())
            return false;
        const Derived &derived = static_cast<const Derived &>(*impl());
        return derived.typeok();
    }

private:
    T *impl_;
};

} /* namespace xslib */

#endif /* XSLIB_COMMON_HPP */
