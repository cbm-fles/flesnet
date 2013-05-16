cmake_minimum_required(VERSION 2.6)

project(flib_control)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")
include_directories("${PROJECT_SOURCE_DIR}/../lib/include")
include_directories("${PROJECT_SOURCE_DIR}/../..")
include_directories("${PROJECT_SOURCE_DIR}/../lib/src")
include_directories("${PROJECT_SOURCE_DIR}/../rorcfs/include")

add_executable(flib_control flib_control.cpp)

target_link_libraries(flib_control pthread)
target_link_libraries(flib_control "${PROJECT_SOURCE_DIR}/../lib/lib/release/librorc.so")
