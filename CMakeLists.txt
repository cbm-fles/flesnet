cmake_minimum_required(VERSION 2.6)
project(flesnet)

set(CMAKE_CXX_FLAGS "-std=gnu++0x -g -Wall -pedantic -O3")

#set(CMAKE_CXX_LINK_FLAGS "-pg")

find_package(Boost 1.40 COMPONENTS program_options thread REQUIRED)

#add_definitions(-DTHREADSAFE)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

add_executable(flesnet main.cpp ComputeApplication.cpp)

target_link_libraries(flesnet rdmacm)
target_link_libraries(flesnet ${Boost_PROGRAM_OPTIONS_LIBRARY})
target_link_libraries(flesnet ${Boost_THREAD_LIBRARY})

# Documentation
include(FindDoxygen)
if(DOXYGEN_FOUND)
    add_subdirectory(doc)
else(DOXYGEN_FOUND)
    message(STATUS "WARNING: Failed to find Doxygen - won't build documentation")
endif(DOXYGEN_FOUND)
