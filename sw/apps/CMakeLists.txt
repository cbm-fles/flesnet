cmake_minimum_required(VERSION 2.6)

project(flib_control)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")
set(LIBFLIB ${PROJECT_SOURCE_DIR}/../lib/build/libflib.a)
include_directories("${PROJECT_SOURCE_DIR}/../lib/include")
include_directories("${PROJECT_SOURCE_DIR}/../rorcfs/include")

add_executable(flib_control flib_control.cpp)
add_executable(first first.cpp)
add_executable(set_bar set_bar.cpp)
add_executable(control_if control_if.cpp)

target_link_libraries(flib_control pthread)
target_link_libraries(flib_control ${LIBFLIB})
target_link_libraries(set_bar pthread)
target_link_libraries(set_bar ${LIBFLIB})
target_link_libraries(control_if pthread)
target_link_libraries(control_if ${LIBFLIB})
target_link_libraries(first boost_program_options)
