cmake_minimum_required (VERSION 2.6) 
project (RDMA)

set (CMAKE_C_FLAGS "-g -std=c99 -D_POSIX_SOURCE -Wall -pedantic")
set (CMAKE_CXX_FLAGS "-g -Wall -pedantic")

FIND_PACKAGE (Boost 1.40 COMPONENTS program_options thread REQUIRED)

#add_definitions(-DTHREADSAFE)

add_executable (ibtsb main.cpp ComputeApplication.cpp)

target_link_libraries (ibtsb rdmacm)
target_link_libraries (ibtsb ${Boost_PROGRAM_OPTIONS_LIBRARY})
target_link_libraries (ibtsb ${Boost_THREAD_LIBRARY})

# Documentation
include(FindDoxygen)
if(DOXYGEN_FOUND)
    add_subdirectory(doc)
else(DOXYGEN_FOUND)
    message(STATUS "WARNING: Failed to find Doxygen - won't build documentation")
endif(DOXYGEN_FOUND)
