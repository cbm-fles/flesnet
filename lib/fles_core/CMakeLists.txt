# Copyright 2013-2015 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/flib_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/crcutil")

add_library(fles_core ${LIB_SOURCES} ${LIB_HEADERS})

target_link_libraries(fles_core fles_ipc)
if(USE_NUMA AND NUMA_FOUND)
  add_definitions(-DHAVE_NUMA)
  target_link_libraries(fles_core ${NUMA_LIBRARY})
endif()
