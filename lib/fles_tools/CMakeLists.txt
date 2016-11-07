# Copyright 2013-2015 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cxx *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/flib_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/crcutil")

add_library(fles_tools ${LIB_SOURCES} ${LIB_HEADERS})

target_link_libraries(fles_tools fles_core fles_ipc)

target_compile_options(fles_tools PUBLIC -Wno-unknown-pragmas)
