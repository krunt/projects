#ifndef XSLIB_INTEGER_HPP
#define XSLIB_INTEGER_HPP

#include <xslib/xscommon.hpp>

namespace xslib {

class Integer: public xscommon<SV, Integer>,
    boost::less_than_comparable<Integer>,
    boost::equality_comparable<Integer>,
    boost::additive<Integer>
{
public:
    Integer(int val)
        : xscommon<SV, Integer>(newSViv(val), false)
    {}

    Integer(SV *sv)
        : xscommon<SV, Integer>(sv, true)
    {}

    Integer clone() const {
        return Integer(getint());
    }

    Integer &operator+=(const Integer &rhs) {
        int result = getint() + rhs.getint();
        SvIV_set(impl(), result);
        return *this;
    }

    Integer &operator-=(const Integer &rhs) {
        int result = getint() - rhs.getint();
        SvIV_set(impl(), result);
        return *this;
    }

    int getint() const {
        return SvIV(impl());
    }

    bool operator<(const Integer &rhs) const {
        return getint() < rhs.getint();
    }
    bool operator==(const Integer &rhs) const {
        return getint() == rhs.getint();
    }

    bool typeok() const {
        return impl() != &PL_sv_undef && SvIOK(impl());
    }
};
}

#endif /* XSLIB_INTEGER_HPP */
