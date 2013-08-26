extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
}

#include "../xslib.h"

#include <boost/bind/bind.hpp>
#include <boost/function.hpp>

typedef xslib::Integer Integer;
typedef xslib::String String;
typedef xslib::Reference Reference;

typedef xslib::Array Array;
typedef xslib::ArrayIterator ArrayIterator;

typedef xslib::HashTable HashTable;
typedef xslib::HashTableIterator HashTableIterator;

MODULE = tests		PACKAGE = tests		

void
test_addition(SV *lhs, SV *rhs)
  PPCODE:
    try {
        Integer left = xslib::typecast<Integer>(lhs);
        Integer right = xslib::typecast<Integer>(rhs);
        Integer result(left.getint());

        result += right;

        make_result(result);
 
    } catch (xslib::bad_cast &e) {
        croak(e.what());
    }


void
test_array(AV *av)
  PPCODE:
    Array arr = xslib::typecast<Array>(av);
    for (ArrayIterator begin = arr.begin(); begin != arr.end(); ++begin) {
        Integer value = xslib::typecast<Integer>(*begin);
        value += 5;
    }
    make_result(arr);


void
test_function(SV *cv0, SV *cv1, SV *cv2, SV *cv3)
  PPCODE:
    xslib::Function<Integer> fn0
        = xslib::typecast<xslib::Function<Integer> >(cv0, sp);
    Integer result = fn0();

    xslib::Function<Integer, Integer> fn1 
        = xslib::typecast<xslib::Function<Integer, Integer> >(cv1, sp);
    result = fn1(Integer(2));

    xslib::Function<Integer, Integer, Integer> fn2 
        = xslib::typecast<xslib::Function<Integer, Integer, Integer> >(cv2, sp);
    result = fn2(Integer(2), Integer(2));

    xslib::Function<Integer, Integer, Integer, Integer> fn3 
        = xslib::typecast<xslib::Function<Integer, Integer, Integer, Integer> >(cv3, sp);
    result = fn3(Integer(2), Integer(2), Integer(2));
    make_result(result);


void
test_bind(SV *cv2)
  PPCODE:
    typedef xslib::Function<Integer, Integer, Integer>::functype functype;
    xslib::Function<Integer, Integer, Integer> fn2 
        = xslib::typecast<xslib::Function<Integer, Integer, Integer> >(cv2, sp);

    Integer result = fn2.to_function()(Integer(2), Integer(3));
    boost::function<functype> fnc = fn2.to_function();
    Integer result2 = boost::bind(fnc, _1, Integer(3))(Integer(3));
    Integer final_result(result2 + result);
    make_result(final_result); 

void
test_reference(SV *sv)
  PPCODE:
    Reference rfnce = xslib::typecast<Reference>(sv);
    Integer v = xslib::typecast<Integer>(rfnce.dereference());
    make_result(v);

void
test_string(SV *sv)
  PPCODE:
    String v = xslib::typecast<String>(sv);
    make_result(v);


void
test_hashtable(HV *hv)
  PPCODE:
    HashTable table = xslib::typecast<HashTable>(hv);
    make_result(table);
