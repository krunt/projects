#ifndef XSLIB_REFERENCE_HPP
#define XSLIB_REFERENCE_HPP

#include <xslib/xscommon.hpp>

namespace xslib {

class Reference: public xscommon<SV, Reference>,
    boost::equality_comparable<Reference>
{
public:
    Reference(SV *rv)
        : xscommon<SV, Reference>(rv, true)
    {}

    template <typename T>
    Reference(T entity)
        : xscommon<SV, Reference>(newSVrv((SV*)entity.impl(), NULL), false)
    {}

    SV *dereference() const {
        return SvRV(impl());
    }

    bool operator==(const Reference &rhs) const {
        return dereference() == rhs.dereference();
    }

    bool typeok() const {
        return impl() != &PL_sv_undef && SvROK(impl());
    }
};
}

#endif /* XSLIB_REFERENCE_HPP */
