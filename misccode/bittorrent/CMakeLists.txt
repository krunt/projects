cmake_minimum_required(VERSION 2.8)
project(btorrent)

find_package(Boost COMPONENTS filesystem REQUIRED)

file(GLOB_RECURSE TORRENT_SOURCES src/*.cc)

include_directories(./)
add_library(btorrent ${TORRENT_SOURCES})

add_subdirectory(tests)

enable_testing()
add_test(test tests/run_tests)
