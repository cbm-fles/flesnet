# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/fles_rdma"
                    "${PROJECT_SOURCE_DIR}/lib/fles_flib"
                    "${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib"
                    "${PROJECT_SOURCE_DIR}/lib/flib_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/flib")

add_executable(flesnet ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(flesnet fles_core fles_rdma fles_flib logging flib ibverbs rdmacm ${Boost_LIBRARIES})
if(LIBNUMA AND USE_LIBNUMA)
    target_link_libraries(flesnet ${LIBNUMA})
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(flesnet pthread rt)
endif()
