cmake_minimum_required(VERSION 2.8)

set (CMAKE_CXX_FLAGS -g)

include_directories(../)

add_executable(dump_torrent dump_torrent.cpp)
target_link_libraries(dump_torrent btorrent)

add_executable(dump_torrent_info dump_torrent_info.cpp)
target_link_libraries(dump_torrent_info btorrent)

add_executable(start_torrent start_torrent.cpp)
target_link_libraries(start_torrent btorrent)

add_executable(generate_torrent generate_torrent.cpp)
target_link_libraries(generate_torrent btorrent)
