# Copyright 2014 Dirk Hutter, Jan de Cuveland

#set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

include_directories(
        "${PROJECT_SOURCE_DIR}/lib/flib"
        "${PROJECT_SOURCE_DIR}/lib"
        "${PROJECT_SOURCE_DIR}/lib/logging")

add_executable(flib_status flib_status.cpp)
add_executable(test_rf test_rf.cpp)
add_executable(simple_server_2 simple_server.cpp)

target_link_libraries(flib_status flib-static)
target_link_libraries(test_rf flib-static)
target_link_libraries(simple_server_2 logging flib-static ${Boost_LIBRARIES})
