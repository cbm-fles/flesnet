# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

add_executable(zmq_demo ${APP_SOURCES} ${APP_HEADERS})

target_compile_options(zmq_demo
  PRIVATE "-Wno-unused-parameter"
  PUBLIC ${ZMQ_CFLAGS_OTHER}
)

target_include_directories(zmq_demo SYSTEM
  PUBLIC ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(zmq_demo
  ${ZMQ_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)
