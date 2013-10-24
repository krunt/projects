#define BOOST_TEST_MODULE btorrent
#include <boost/test/unit_test.hpp>
#include <include/value.h>

BOOST_AUTO_TEST_SUITE(testing_values)

BOOST_AUTO_TEST_CASE(value_common) {
    btorrent::value_t v;
    btorrent::value_t v1;
    btorrent::value_t v2;
    btorrent::value_t v3;
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_SUITE_END()
