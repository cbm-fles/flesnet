# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

add_library(shm_ipc INTERFACE)

target_include_directories(shm_ipc
  INTERFACE .
)

target_include_directories(shm_ipc SYSTEM
  INTERFACE ${ZMQ_INCLUDE_DIRS}
  INTERFACE ${PROJECT_SOURCE_DIR}/external/cppzmq
)

target_compile_options(shm_ipc
  INTERFACE ${ZMQ_CFLAGS_OTHER}
)

target_link_libraries(shm_ipc
  INTERFACE ${ZMQ_LIBRARIES}
)

add_executable(shm_ipc_demo ${APP_SOURCES} ${APP_HEADERS})

target_compile_options(shm_ipc_demo
  PRIVATE "-Wno-unused-parameter"
)

target_link_libraries(shm_ipc_demo
  shm_ipc
  ${CMAKE_THREAD_LIBS_INIT}
)
