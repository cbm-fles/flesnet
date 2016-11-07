# Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
# Copyright 2016 Pierre-Alain Loizeau <p.-a.loizeau@gsi.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.h)

list(APPEND APP_SOURCES "${CMAKE_BINARY_DIR}/config/GitRevision.cpp")
list(APPEND APP_HEADERS "${PROJECT_SOURCE_DIR}/config/GitRevision.hpp")

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/flib_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/crcutil"
                    "${PROJECT_SOURCE_DIR}/lib/fles_tools"
                    "${PROJECT_SOURCE_DIR}/config")

add_executable(ngdpbtool ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(ngdpbtool fles_ipc fles_core logging crcutil fles_tools ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(ngdpbtool ${ZMQ_LIBRARIES})
if(LIBNUMA AND USE_LIBNUMA)
    target_link_libraries(ngdpbtool ${LIBNUMA})
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(ngdpbtool rt atomic)
endif()
