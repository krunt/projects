cmake_minimum_required(VERSION 2.8)
project(btorrent)

find_package(Boost COMPONENTS date_time filesystem regex system REQUIRED)

set (CMAKE_CXX_FLAGS -g)

file(GLOB_RECURSE TORRENT_SOURCES src/*.cc)

include_directories(./)
add_library(btorrent ${TORRENT_SOURCES})

if(Boost_FOUND)
    target_link_libraries(btorrent ${Boost_LIBRARIES})
endif()

add_subdirectory(support)
add_subdirectory(tests)

enable_testing()
add_test(test tests/run_tests)
