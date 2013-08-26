#define MMDEFAULT(n, data) data ## n = NoneType
#define MACRODEFAULT(z, n, data) BOOST_PP_COMMA() \
 BOOST_PP_IF(BOOST_PP_LESS(n, NUMARGS), BOOST_PP_CAT(data, n), MMDEFAULT(n, data))
#define ARGMACRO(z, n, data) \
    BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(n, 0)) data ## n arg ## n

#define PRELEFT xpushs_shim(sp, sv2mortal_shim(sp, 
#define PRERIGHT .dispose()));
#define PUSHARGUMENTMACRO(z, n, data) PRELEFT data ## n PRERIGHT

#define MAYBECOMMA BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(NUMARGS, 0))
#define MAYBEPUTBACK BOOST_PP_IF(BOOST_PP_NOT_EQUAL(NUMARGS, 0), PUTBACK;, )

#define SPECSUBST <R MAYBECOMMA BOOST_PP_ENUM_PARAMS(NUMARGS, T)>

template <typename R MAYBECOMMA BOOST_PP_ENUM_PARAMS(NUMARGS, typename T)>

#if NUMARGS != MAXARITY
class Function<R MAYBECOMMA BOOST_PP_ENUM_PARAMS(NUMARGS, T)>
#else
class Function
#endif

: public xscommon<SV, 
        Function<R MAYBECOMMA BOOST_PP_ENUM_PARAMS(NUMARGS, T)> >, 
    public FunctionBase<Function<R MAYBECOMMA BOOST_PP_ENUM_PARAMS(NUMARGS, T)> >
{
public:
    typedef R (functype)(BOOST_PP_ENUM_PARAMS(NUMARGS, T));
    typedef Function<R MAYBECOMMA BOOST_PP_ENUM_PARAMS(NUMARGS, T)> this_type;
    typedef R return_value_type;

    using FunctionBase<this_type>::getsp;
    using FunctionBase<this_type>::prepare_call_state;
    using FunctionBase<this_type>::finish_call_state;
    using xscommon<SV, this_type>::impl;

public:
    Function(SV *cv, SV **&sp)
        : xscommon<SV, this_type>(cv, true),
          FunctionBase<this_type>(sp)
    {}

    return_value_type operator()(BOOST_PP_REPEAT(NUMARGS, ARGMACRO, T)) {
        SV **&sp = getsp();
        prepare_call_state();

        PUSHMARK(SP);
        BOOST_PP_REPEAT(NUMARGS, PUSHARGUMENTMACRO, arg)
        MAYBEPUTBACK 

        checkarity(call_sv(impl(), G_SCALAR), 1);
        SPAGAIN;
        return_value_type result = return_value_type(POPs).clone();

        finish_call_state();
        return result;
    }

    boost::function<functype> to_function() {
        return boost::function<functype>(*this);
    }
};

#undef MMDEFAULT
#undef MACRODEFAULT
#undef ARGMACRO
#undef PRELEFT
#undef PRERIGHT
#undef MAYBECOMMA
#undef MAYBEPUTBACK

