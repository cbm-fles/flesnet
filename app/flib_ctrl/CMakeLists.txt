# Copyright 2014 Dirk Hutter, Jan de Cuveland

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

include_directories("${PROJECT_SOURCE_DIR}/lib/einhard")
include_directories("${PROJECT_SOURCE_DIR}/lib/flib")
include_directories("${PROJECT_SOURCE_DIR}/lib/flib_server")
include_directories("${PROJECT_SOURCE_DIR}/ctrl")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=1)

add_executable(flib_ctrl flib_ctrl.cpp)

target_link_libraries(flib_ctrl flib zmq ${Boost_LIBRARIES})
target_link_libraries(flib_ctrl ${PROJECT_SOURCE_DIR}/ctrl/lib/libcbmnetcntlserver.so)
