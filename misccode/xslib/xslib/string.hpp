#ifndef XSLIB_STRING_HPP
#define XSLIB_STRING_HPP

#include <xslib/xscommon.hpp>
#include <string>

namespace xslib {

class String: public xscommon<SV, String>, boost::less_than_comparable<String> {
public:
    String(std::string val)
        : xscommon<SV, String>(newSVpv(val.data(), val.length()), false)
    {}

    String(SV *sv)
        : xscommon<SV, String>(sv, true)
    {}

    String clone() const {
        return String(getstr());
    }

    std::string getstr() const {
        STRLEN len;
        char *str = SvPV(impl(), len);
        return std::string(str);
    }

    bool operator<(const String &rhs) const {
        return std::lexicographical_compare(
                getstr().begin(), getstr().end(),
                rhs.getstr().begin(), rhs.getstr().end());
    }

    bool operator==(const String &rhs) const {
        return getstr() == rhs.getstr();
    }

    bool typeok() const {
        return impl() != &PL_sv_undef && SvPOK(impl());
    }
};

}

#endif /* XSLIB_STRING_HPP */
