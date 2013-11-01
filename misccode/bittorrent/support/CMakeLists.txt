cmake_minimum_required(VERSION 2.8)

include_directories(../)

add_executable(dump_torrent dump_torrent.cpp)
target_link_libraries(dump_torrent btorrent)
