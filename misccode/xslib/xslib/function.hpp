#ifndef XSLIB_FUNCTION_HPP
#define XSLIB_FUNCTION_HPP

#include <xslib/xscommon.hpp>
#include <boost/function.hpp>

#include <boost/preprocessor/repetition.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/arithmetic.hpp>
#include <boost/preprocessor/comparison/less.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/control.hpp>
#include <boost/preprocessor/repetition.hpp>

namespace xslib {

class invalid_arity: std::exception {
};

template <typename Derived>
class FunctionBase {
public:
    FunctionBase(SV ** &sp) 
        : stacksp(sp)
    {}

    bool typeok() const {
        Derived func = static_cast<Derived&>(*this);
        return func.impl() != &PL_sv_undef && SvTYPE(func.impl()) == SVt_PVCV;
    }

protected:
    SV **&getsp() { 
        return stacksp;
    }

    void prepare_call_state() {
        SV **&sp = getsp();

        ENTER;
        SAVETMPS;
    }

    void finish_call_state() {
        SV **&sp = getsp();

        PUTBACK;
        FREETMPS;
        LEAVE;
    }

    void checkarity(int result_value, int true_value) const {
        if (result_value != true_value) {
            throw invalid_arity();
        }
    }

private:
    SV **&stacksp;
};

#define MAXARITY 10

struct NoneType {};

#define MMDEFAULT(n, data) data ## n = NoneType
#define MACRODEFAULT(z, n, data) BOOST_PP_COMMA() MMDEFAULT(n, data)

template <typename R BOOST_PP_REPEAT(MAXARITY, MACRODEFAULT, typename T)>
class Function;

static void xpushs_shim(SV **&sp, SV *sv) { XPUSHs(sv); } 
static SV *sv2mortal_shim(SV **&sp, SV *sv) { return sv_2mortal(sv); } 

#undef MACRODEFAULT
#undef MMDEFAULT

#define NUMARGS 0
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 1
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 2
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 3
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 4
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 5
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 6
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 7
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 8
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 9
#include "detail/function.hpp"
#undef NUMARGS

#define NUMARGS 10
#include "detail/function.hpp"
#undef NUMARGS

}

#endif /* XSLIB_FUNCTION_HPP */
