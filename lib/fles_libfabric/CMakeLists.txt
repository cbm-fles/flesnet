# Copyright 2016 Thorsten Schuett <schuett@zib.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/logging")

add_library(fles_libfabric ${LIB_SOURCES} ${LIB_HEADERS})
