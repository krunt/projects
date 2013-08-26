#ifndef XSLIB_RESULT_HPP
#define XSLIB_RESULT_HPP

#include <xslib/xscommon.hpp>

namespace xslib {

template <typename T>
class Return;

template <>
class Return<Integer> {
public:
    void operator()(SV **&sp, Integer &result) {
        EXTEND(sp, 1);        
        PUSHs(sv_2mortal(result.dispose()));
    }
};

template <>
class Return<Array> {
public:
    void operator()(SV **&sp, Array &result) {
        EXTEND(sp, result.length());
        for (ArrayIterator it = result.begin(); it != result.end(); ++it) {
            PUSHs(*it);
        }
    }
};

template <>
class Return<Reference> {
public:
    void operator()(SV **&sp, Reference &result) {
        EXTEND(sp, 1);
        PUSHs(sv_2mortal(result.dispose()));
    }
};

template <>
class Return<String> {
public:
    void operator()(SV **&sp, String &result) {
        EXTEND(sp, 1);
        PUSHs(sv_2mortal(result.dispose()));
    }
};

template <>
class Return<HashTable> {
public:
    void operator()(SV **&sp, HashTable &result) {
        EXTEND(sp, result.length() * 2);
        for (HashTableIterator it = result.begin(); it != result.end(); ++it) {
            PUSHs(String((*it).first).dispose());
            PUSHs((*it).second);
        }
    }
};

}

#endif /* XSLIB_RESULT_HPP */
