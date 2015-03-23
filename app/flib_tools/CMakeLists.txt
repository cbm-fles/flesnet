# Copyright 2014 Dirk Hutter, Jan de Cuveland

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

include_directories(
        "${PROJECT_SOURCE_DIR}/lib/flib2"
        "${PROJECT_SOURCE_DIR}/lib"
        "${PROJECT_SOURCE_DIR}/lib/logging")

add_executable(flib_status flib_status.cpp)

target_link_libraries(flib_status flib-static2)
