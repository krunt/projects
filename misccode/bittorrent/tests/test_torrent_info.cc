
#include <boost/test/unit_test.hpp>
#include <include/torrent_info.h>

BOOST_AUTO_TEST_SUITE(testing_torrent_info)

BOOST_AUTO_TEST_CASE(torrent_info_test_single_file_case) {
    using namespace btorrent;
    torrent_info_t t = construct_torrent_info("torrents/taxi_driver.torrent");
    BOOST_CHECK_EQUAL(t.m_announce_url, "udp://bt.rutor.org:2710/announce");

    std::vector<std::string> announce_list;
    announce_list.push_back("udp://bt.rutor.org:2710/announce");
    announce_list.push_back("http://retracker.local/announce");

    BOOST_CHECK_EQUAL_COLLECTIONS(t.m_announce_list.begin(), 
        t.m_announce_list.end(), announce_list.begin(), announce_list.end());
    BOOST_CHECK_EQUAL(t.m_piece_size, 4194304);
}

BOOST_AUTO_TEST_SUITE_END()
