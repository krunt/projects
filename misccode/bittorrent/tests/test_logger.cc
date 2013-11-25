#include <boost/test/unit_test.hpp>
#include <include/logger.h>

BOOST_AUTO_TEST_SUITE(testing_logger)

BOOST_AUTO_TEST_CASE(logger_compilation) {
    btorrent::logger_t logger;
    logger.info("hi");
    logger.warn("hi");
    logger.error("hi");
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_SUITE_END()
