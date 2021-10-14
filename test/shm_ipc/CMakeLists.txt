# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

set(APP_SOURCES
  shm_ipc_demo.cpp
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
