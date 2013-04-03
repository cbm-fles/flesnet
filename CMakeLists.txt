cmake_minimum_required(VERSION 2.6)
project(flesnet)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -pedantic -O3")

find_package(Boost 1.40 COMPONENTS program_options thread REQUIRED)

#add_definitions(-DTHREADSAFE)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

add_custom_command(
        OUTPUT flesnet.cfg
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_CURRENT_SOURCE_DIR}/contrib/flesnet.cfg flesnet.cfg
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/flesnet.cfg)

add_custom_command(
        OUTPUT run
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_CURRENT_SOURCE_DIR}/contrib/run run
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/run)

add_custom_command(
        OUTPUT verbs.supp
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_CURRENT_SOURCE_DIR}/contrib/verbs.supp verbs.supp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/verbs.supp)

add_executable(flesnet main.cpp flesnet.cfg run verbs.supp)

target_link_libraries(flesnet ibverbs)
target_link_libraries(flesnet rdmacm)
target_link_libraries(flesnet rt)
target_link_libraries(flesnet ${Boost_PROGRAM_OPTIONS_LIBRARY})
target_link_libraries(flesnet ${Boost_THREAD_LIBRARY})

# Documentation
include(FindDoxygen)
if(DOXYGEN_FOUND)
    add_subdirectory(doc)
else(DOXYGEN_FOUND)
    message(STATUS "WARNING: Failed to find Doxygen - won't build documentation")
endif(DOXYGEN_FOUND)
