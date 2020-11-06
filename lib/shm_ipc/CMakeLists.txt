# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

add_executable(shm_ipc_demo ${APP_SOURCES} ${APP_HEADERS})

target_compile_options(shm_ipc_demo
  PRIVATE "-Wno-unused-parameter"
  PUBLIC ${ZMQ_CFLAGS_OTHER}
)

target_include_directories(shm_ipc_demo SYSTEM
  PUBLIC ${ZMQ_INCLUDE_DIRS}
  PUBLIC ${PROJECT_SOURCE_DIR}/external/cppzmq
)

target_link_libraries(shm_ipc_demo
  ${ZMQ_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)
