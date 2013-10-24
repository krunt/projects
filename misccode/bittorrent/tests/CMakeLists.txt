cmake_minimum_required(VERSION 2.8)

file(GLOB_RECURSE TEST_SOURCES *.cc)
find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories(../)

add_definitions(-DBOOST_TEST_DYN_LINK)

set(TEST run_tests)
add_executable(${TEST} ${TEST_SOURCES})
target_link_libraries(${TEST} btorrent ${Boost_LIBRARIES})
