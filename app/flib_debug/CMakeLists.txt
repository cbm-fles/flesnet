# Copyright 2014 Dirk Hutter, Jan de Cuveland

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

include_directories("${PROJECT_SOURCE_DIR}/lib/flib/include" "${PROJECT_SOURCE_DIR}/lib/einhard")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

add_executable(set_bar set_bar.cpp)
add_executable(control_if control_if.cpp)
add_executable(simple_server simple_server.cpp)

target_link_libraries(set_bar flib-static)
target_link_libraries(control_if flib-static)
target_link_libraries(simple_server flib-static)
