# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

set(LIB_SOURCES
  ItemDistributor.cpp
)

set(LIB_HEADERS
  ItemDistributor.hpp
  ItemDistributorWorker.hpp
  ItemID.hpp
  ItemProducer.hpp
  ItemWorker.hpp
  ItemWorkerProtocol.hpp
)

add_library(shm_ipc ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(shm_ipc
  PUBLIC .
)

target_link_libraries(shm_ipc
  PUBLIC logging
  PUBLIC zmq::cppzmq
)

install(TARGETS shm_ipc DESTINATION lib COMPONENT dev)
install(FILES ${LIB_HEADERS} DESTINATION include COMPONENT dev)
