cmake_minimum_required(VERSION 2.6)

project(flib_ctrl)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")
set(CTRL_LIBRARY_PATH "${PROJECT_SOURCE_DIR}/../ctrl")


include_directories("${PROJECT_SOURCE_DIR}/lib/")
include_directories("${CTRL_LIBRARY_PATH}")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=1)

add_executable(flib_ctrl flib_ctrl.cpp)

target_link_libraries(flib_ctrl flib)
target_link_libraries(flib_ctrl boost_thread)
target_link_libraries(flib_ctrl zmq)
target_link_libraries(flib_ctrl ${CTRL_LIBRARY_PATH}/lib/libcbmnetcntlserver.so)
