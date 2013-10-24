
#include <boost/test/unit_test.hpp>
#include <include/bencode.h>

BOOST_AUTO_TEST_SUITE(testing_bencode)

BOOST_AUTO_TEST_CASE(bencode_common) {
    using namespace btorrent;
    value_t v1(2);
    value_t v2("2");
    value_t::list_type v31;
    v31.push_back(v1); 
    v31.push_back(v2); 
    value_t v3(v31);
    value_t::dictionary_type v41;
    v41["1"] = v1;
    v41["2"] = v2;
    value_t v4(v41);

    BOOST_CHECK_EQUAL(bencode(v1), "i2e");
    BOOST_CHECK_EQUAL(bencode(v2), "1:2");
    BOOST_CHECK_EQUAL(bencode(v3), "li2e1:2e");
    BOOST_CHECK_EQUAL(bencode(v4), "d1:1i2e1:21:2e");
}

BOOST_AUTO_TEST_CASE(bdecode_common) {
    using namespace btorrent;
    BOOST_CHECK_EQUAL(bencode(bdecode("i2e")), "i2e");
    BOOST_CHECK_EQUAL(bencode(bdecode("1:2")), "1:2");
    BOOST_CHECK_EQUAL(bencode(bdecode("li2e1:2e")), "li2e1:2e");
    BOOST_CHECK_EQUAL(bencode(bdecode("d1:1i2e1:21:2e")), "d1:1i2e1:21:2e");
}

BOOST_AUTO_TEST_SUITE_END()
