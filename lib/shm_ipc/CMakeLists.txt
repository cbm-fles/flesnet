# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

set(LIB_SOURCES
  ItemDistributor.cpp
)

set(LIB_HEADERS
  ItemDistributor.hpp
  ItemProducer.hpp
  ItemWorker.hpp
  ItemWorkerProtocol.hpp
)

add_library(shm_ipc ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(shm_ipc
  PUBLIC .
)

target_include_directories(shm_ipc SYSTEM
  PUBLIC ${ZMQ_INCLUDE_DIRS}
  PUBLIC ${PROJECT_SOURCE_DIR}/external/cppzmq
)

target_compile_options(shm_ipc
  PUBLIC ${ZMQ_CFLAGS_OTHER}
)

target_link_libraries(shm_ipc
  PUBLIC ${ZMQ_LIBRARIES}
)

set(APP_SOURCES
  main.cpp
)

set(APP_HEADERS
  ExampleProducer.hpp
  ExampleWorker.hpp
)

add_executable(shm_ipc_demo ${APP_SOURCES} ${APP_HEADERS})

target_compile_options(shm_ipc_demo
  PRIVATE "-Wno-unused-parameter"
)

target_link_libraries(shm_ipc_demo
  shm_ipc
  ${CMAKE_THREAD_LIBS_INIT}
)
