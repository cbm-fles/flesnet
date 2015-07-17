# Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc/src"
                    "${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/logging")

add_library(fles_rdma ${LIB_SOURCES} ${LIB_HEADERS})
