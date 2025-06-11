# Copyright 2015-2025 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_ucx ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(fles_ucx PUBLIC .)

target_include_directories(fles_ucx SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(fles_ucx
  PUBLIC fles_ipc
  PUBLIC fles_core
  PUBLIC logging
  PUBLIC monitoring
  PUBLIC ${RDMA_LIBRARIES}
  PUBLIC ucx::ucp
  PUBLIC ucx::uct
  PUBLIC ucx::ucs
)
