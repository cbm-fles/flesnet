# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

add_executable(zmq_demo ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(zmq_demo SYSTEM
  PUBLIC ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(zmq_demo
  PUBLIC ${ZMQ_LIBRARIES}
)
