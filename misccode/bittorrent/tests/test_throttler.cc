#include <boost/test/unit_test.hpp>
#include <include/throttler.h>
#include <unistd.h>

BOOST_AUTO_TEST_SUITE(testing_throttler)

BOOST_AUTO_TEST_CASE(throttler) {
    btorrent::throttler_t throttler(2, 1024);
    BOOST_CHECK_EQUAL(throttler.available(), 1024);
    throttler.account(512);
    BOOST_CHECK_EQUAL(throttler.available(), 512);
    throttler.account(512);
    BOOST_CHECK_EQUAL(throttler.available(), 0);
    BOOST_CHECK_EQUAL(throttler.available(), 0);
}

BOOST_AUTO_TEST_CASE(throttler_expired) {
    btorrent::throttler_t throttler(1, 1024);
    BOOST_CHECK_EQUAL(throttler.available(), 1024);
    throttler.account(512);
    BOOST_CHECK_EQUAL(throttler.available(), 512);
    sleep(1);
    usleep(1000 * 1000);
    BOOST_CHECK_EQUAL(throttler.available(), 1024);
}

BOOST_AUTO_TEST_SUITE_END()
