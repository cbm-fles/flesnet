# Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/flib"
                    "${PROJECT_SOURCE_DIR}/lib")

add_library(fles_flib ${LIB_SOURCES} ${LIB_HEADERS})
