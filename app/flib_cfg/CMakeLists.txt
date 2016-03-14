# Copyright 2014 Dirk Hutter, Jan de Cuveland

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.hpp)

include_directories("${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/flib"
                    "${PROJECT_SOURCE_DIR}/lib")

add_executable(flib_cfg flib_cfg.cpp parameters.hpp)

target_link_libraries(flib_cfg flib logging ${Boost_LIBRARIES} pthread rt)
