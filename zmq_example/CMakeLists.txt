# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

add_executable(zmq_example ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(zmq_example SYSTEM
  PUBLIC ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(zmq_example
  PUBLIC ${ZMQ_LIBRARIES}
)
