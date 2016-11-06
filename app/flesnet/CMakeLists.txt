# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/fles_rdma"
                    "${PROJECT_SOURCE_DIR}/lib/fles_zeromq"
                    "${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib"
                    "${PROJECT_SOURCE_DIR}/lib/flib_ipc")

add_executable(flesnet ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(flesnet fles_core fles_rdma fles_zeromq flib_ipc logging ibverbs rdmacm ${Boost_LIBRARIES})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(flesnet pthread rt atomic)
endif()
