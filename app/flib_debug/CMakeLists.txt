# Copyright 2014 Dirk Hutter, Jan de Cuveland

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

include_directories("${PROJECT_SOURCE_DIR}/lib/flib" "${PROJECT_SOURCE_DIR}/lib/logging")

add_executable(control_if control_if.cpp)
add_executable(simple_server simple_server.cpp)
add_executable(dlm dlm.cpp)
add_executable(dcm_test dcm_test.cpp)
add_executable(link_status link_status.cpp)
add_executable(link_reset link_reset.cpp)

target_link_libraries(control_if flib-static)
target_link_libraries(simple_server logging flib-static ${Boost_LIBRARIES})
target_link_libraries(dlm flib-static)
target_link_libraries(dcm_test flib-static)
target_link_libraries(link_status flib-static)
target_link_libraries(link_reset flib-static)
