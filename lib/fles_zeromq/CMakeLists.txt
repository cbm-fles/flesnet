# Copyright 2015-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_zeromq ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(fles_zeromq PUBLIC .)

target_include_directories(fles_zeromq SYSTEM PUBLIC ${ZMQ_INCLUDE_DIRS})

target_link_libraries(fles_zeromq
  PUBLIC fles_ipc
  PUBLIC fles_core
  PUBLIC logging
  PUBLIC ${ZMQ_LIBRARIES}
)
