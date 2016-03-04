# Copyright 2014 Dirk Hutter, Jan de Cuveland

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

include_directories("${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/fles_ipc/src"
                    "${PROJECT_SOURCE_DIR}/lib/flib"
                    "${PROJECT_SOURCE_DIR}/lib/flib_ipc"
                    "${PROJECT_SOURCE_DIR}/lib")

set(SERVER_LIB
        shm_device_server.hpp
        shm_channel_server.hpp
        ${PROJECT_SOURCE_DIR}/lib/flib_ipc/shm_device.hpp
        ${PROJECT_SOURCE_DIR}/lib/flib_ipc/shm_channel.hpp)

set(CLIENT_LIB
        ${PROJECT_SOURCE_DIR}/lib/flib_ipc/shm_device_client.hpp
        ${PROJECT_SOURCE_DIR}/lib/flib_ipc/shm_channel_client.hpp
        ${PROJECT_SOURCE_DIR}/lib/flib_ipc/shm_device.hpp
        ${PROJECT_SOURCE_DIR}/lib/flib_ipc/shm_channel.hpp)

add_executable(flib_server flib_server.cpp ${SERVER_LIB})
add_executable(simple_consumer simple_consumer.cpp ${CLIENT_LIB})

target_compile_definitions(flib_server
        PRIVATE -DBOOST_INTERPROCESS_ENABLE_TIMEOUT_WHEN_LOCKING
                -DBOOST_INTERPROCESS_TIMEOUT_WHEN_LOCKING_DURATION_MS=5000)

target_link_libraries(flib_server flib logging ${Boost_LIBRARIES} rt)
target_link_libraries(simple_consumer logging ${Boost_LIBRARIES} rt)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(flib_server pthread)
    target_link_libraries(simple_consumer pthread)
endif()

