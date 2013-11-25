#include <boost/test/unit_test.hpp>
#include <include/utils.h>

BOOST_AUTO_TEST_SUITE(testing_utils)

BOOST_AUTO_TEST_CASE(parsing_url_t) {
    btorrent::url_t urls[] = {
        btorrent::url_t("http://x.ru:80/xyz"),
        btorrent::url_t("udp://x.ru:9999/xyz"),
        btorrent::url_t("http://x.ru/xyz"),
        btorrent::url_t("http://x.ru"),
        btorrent::url_t("udp://x.ru:9999"),
    };

    BOOST_CHECK_EQUAL(urls[0].get_scheme(), "http");
    BOOST_CHECK_EQUAL(urls[0].get_host(), "x.ru");
    BOOST_CHECK_EQUAL(urls[0].get_port(), 80);
    BOOST_CHECK_EQUAL(urls[0].get_path(), "/xyz");
}

BOOST_AUTO_TEST_SUITE_END()
