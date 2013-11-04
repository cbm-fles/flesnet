cmake_minimum_required(VERSION 2.6)

project(flib_control)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")
include_directories("${PROJECT_SOURCE_DIR}/../lib/include")
include_directories("${PROJECT_SOURCE_DIR}/../flibctrl/lib/")
include_directories("${PROJECT_SOURCE_DIR}/../../../usbdaq-test/cbmnet/")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

add_subdirectory("${PROJECT_SOURCE_DIR}/../lib" "${PROJECT_SOURCE_DIR}/../lib/build")

add_executable(flib_control flib_control.cpp)
#add_executable(first first.cpp)
add_executable(set_bar set_bar.cpp)
add_executable(control_if control_if.cpp)
add_executable(simple_server simple_server.cpp)

target_link_libraries(flib_control pthread)
target_link_libraries(flib_control flib-static)
target_link_libraries(set_bar pthread)
target_link_libraries(set_bar flib-static)
target_link_libraries(control_if pthread)
target_link_libraries(control_if flib-static)
#target_link_libraries(first boost_program_options)

target_link_libraries(simple_server flib-static)
target_link_libraries(simple_server boost_thread)
target_link_libraries(simple_server zmq)
target_link_libraries(simple_server ${PROJECT_SOURCE_DIR}/../../../usbdaq-test/cbmnet/lib/libcbmnetcntlserver.so)
