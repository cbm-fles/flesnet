# Copyright 2014 Dirk Hutter, Jan de Cuveland

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.hpp)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

include_directories("${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/flib"
                    "${PROJECT_SOURCE_DIR}/lib")

add_executable(flib_cfg flib_cfg.cpp parameters.hpp)

target_link_libraries(flib_cfg flib logging ${ZMQ_LIBRARIES} ${Boost_LIBRARIES} rt)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(flib_cfg pthread gssapi_krb5)
endif()

