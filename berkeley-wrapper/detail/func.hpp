/*
    INPUT_PARAMETER: ARITY (NUMBER)
*/

#define LEFTPART1 typedef typename mpl::at<IndexFields, mpl::int_<
#define RIGHTPART1 > >::type 
#define ATMACRO(z, n, data) LEFTPART1 n RIGHTPART1 BOOST_PP_CAT(data, BOOST_PP_INC(n));

#define PARAMS_MACRO(z, n, data) \
    BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(n, 0)) \
    BOOST_PP_CAT(data, BOOST_PP_INC(n)) & BOOST_PP_CAT(t, BOOST_PP_INC(n))

#define RIGHTPART2 .packed_length()
#define PACK_KEY_LENGTH_MACRO(z, n, data) \
    + BOOST_PP_CAT(data, BOOST_PP_INC(n)) RIGHTPART2

#define RIGHTPART3 .pack(p);
#define PACK_MACRO(z, n, data) \
     BOOST_PP_CAT(data, BOOST_PP_INC(n)) RIGHTPART3

#define RIGHTPART4 .unpack(p);
#define UNPACK_MACRO(z, n, data) \
     BOOST_PP_CAT(data, BOOST_PP_INC(n)) RIGHTPART4

#define LEFTPART5 vec.push_back(
#define UNPACK_VECTOR_MACRO(z, n, data) \
    LEFTPART5 BOOST_PP_CAT(data, BOOST_PP_INC(n)) ());

#define INSERTLIST_MACRO(z, n, data) \
    BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(n, 0)) BOOST_PP_CAT(data, BOOST_PP_INC(n))

template <typename IndexFields>
class TableImpl<ARITY, IndexFields>: public BdbImpl<IndexFields> {
public:
    BOOST_PP_REPEAT(ARITY, ATMACRO, ArgumentType)
    typedef typename Predicate<IndexFields>::fieldlist_type fieldlist_type;

public:
    TableImpl(const std::string &filename)
        : BdbImpl<IndexFields>(filename)
    {}

    int packed_key_length(BOOST_PP_REPEAT(ARITY, PARAMS_MACRO, const ArgumentType)) {
        return BOOST_PP_REPEAT(ARITY, PACK_KEY_LENGTH_MACRO, t);
    }

    void pack_key(BOOST_PP_REPEAT(ARITY, PARAMS_MACRO, const ArgumentType), String &key) {
        char *p = key.data();
        BOOST_PP_REPEAT(ARITY, PACK_MACRO, p = t)
        key.set_length(p - key.data());
    }

    void unpack_key(BOOST_PP_REPEAT(ARITY, PARAMS_MACRO, ArgumentType), 
            const String &key) 
    {
        char *p = key.data();
        BOOST_PP_REPEAT(ARITY, UNPACK_MACRO, p = t)
    }

    void unpack_key(fieldlist_type &vec, const String &key) {
        if (vec.empty()) {
            BOOST_PP_REPEAT(ARITY, UNPACK_VECTOR_MACRO, new ArgumentType)
        }
        char *p = key.data();
        for (int i = 0; i < vec.size(); ++i) {
            p = vec[i]->unpack(p);
        }
        assert(p - key.data() == key.length());
    }

    int compare_keys(const String &lhs, const String &rhs) {
        fieldlist_type lhsvector;
        fieldlist_type rhsvector;
        unpack_key(lhsvector, lhs);
        unpack_key(rhsvector, rhs);

        int cmp_result = 0;
        for (int i = 0; i < lhsvector.size(); ++i) {
            if (cmp_result = lhsvector[i]->compare(*rhsvector[i])) {
                break;
            }
        }

        dispose_fieldlist_shim<IndexFields>(lhsvector);
        dispose_fieldlist_shim<IndexFields>(rhsvector);

        return cmp_result;
    }

    void insert(BOOST_PP_REPEAT(ARITY, PARAMS_MACRO, const ArgumentType), 
            const String &value) 
    {
        enlarge_key_if_needed(
            packed_key_length(BOOST_PP_REPEAT(ARITY, INSERTLIST_MACRO, t)));
        pack_key(BOOST_PP_REPEAT(ARITY, INSERTLIST_MACRO, t), this->cached_key());
        insert_impl(this->cached_key(), value);
    }
};

#undef LEFTPART1
#undef RIGHTPART1
#undef ATMACRO
#undef PARAMS_MACRO
#undef RIGHTPART2
#undef PACK_KEY_LENGTH_MACRO
#undef RIGHTPART3
#undef PACK_MACRO
#undef RIGHTPART4
#undef UNPACK_MACRO
#undef RIGHTPART5
#undef UNPACK_VECTOR_MACRO
#undef INSERTLIST_MACRO
