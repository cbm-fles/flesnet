# Copyright 2013, 2015, 2016 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc"
                    "${PROJECT_SOURCE_DIR}/lib/fles_core"
                    "${PROJECT_SOURCE_DIR}/lib/logging")

set(LIB_SOURCES
    shm_channel_client.cpp
    shm_device_client.cpp
    shm_channel_provider.cpp
    shm_device_provider.cpp
)

set(LIB_HEADERS
    shm_channel_client.hpp
    shm_channel.hpp
    shm_device_client.hpp
    shm_device.hpp
    shm_channel_provider.hpp
    shm_device_provider.hpp
)

add_library(flib_ipc ${LIB_SOURCES} ${LIB_HEADERS})
