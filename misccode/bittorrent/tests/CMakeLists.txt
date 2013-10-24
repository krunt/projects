cmake_minimum_required(VERSION 2.8)

set (TEST_SOURCES test_values.cc)
find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories(../)

set(TEST test_${PROJECT})
add_executable(${TEST} ${TEST_SOURCES})
target_link_libraries(${TEST} ${Boost_LIBRARIES})

enable_testing()
add_test(${TEST} ${TEST})
