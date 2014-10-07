# Copyright 2013-2014 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/app/flesnet"
                    "${PROJECT_SOURCE_DIR}/lib/flib"
                    "${PROJECT_SOURCE_DIR}/lib/einhard"
                    "${PROJECT_SOURCE_DIR}/lib/logging")

add_library(fles_core ${LIB_SOURCES} ${LIB_HEADERS})
