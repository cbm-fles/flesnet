# Copyright 2015-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_rdma ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(fles_rdma PUBLIC .)

target_include_directories(fles_rdma SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(fles_rdma
  PUBLIC fles_ipc
  PUBLIC fles_core
  PUBLIC logging
  PUBLIC influxdb
  PUBLIC ${RDMA_LIBRARIES}
)
