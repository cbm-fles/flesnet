cmake_minimum_required (VERSION 2.6) 
project (RDMA)

set (CMAKE_C_FLAGS "-std=c99 -D_POSIX_SOURCE -Wall -pedantic")
set (CMAKE_CXX_FLAGS "-Wall -pedantic")

FIND_PACKAGE (Boost 1.40 COMPONENTS program_options REQUIRED)

add_executable (ibtsb ibtsb.cpp Parameters.cpp)

target_link_libraries (ibtsb rdmacm) 
target_link_libraries (ibtsb ${Boost_PROGRAM_OPTIONS_LIBRARY}) 
