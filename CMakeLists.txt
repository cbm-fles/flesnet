cmake_minimum_required (VERSION 2.6) 
project (RDMA)

set (CMAKE_C_FLAGS "-g -std=c99 -D_POSIX_SOURCE -Wall -pedantic")
set (CMAKE_CXX_FLAGS "-g -Wall -pedantic")

FIND_PACKAGE (Boost 1.40 COMPONENTS program_options thread REQUIRED)

add_definitions(-DTHREADSAFE)

add_executable (ibtsb main.cpp Parameters.cpp InputApplication.cpp ComputeApplication.cpp log.cpp)

target_link_libraries (ibtsb rdmacm) 
target_link_libraries (ibtsb ${Boost_PROGRAM_OPTIONS_LIBRARY}) 
target_link_libraries (ibtsb ${Boost_THREAD_LIBRARY}) 
