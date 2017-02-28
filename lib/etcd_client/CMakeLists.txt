# Copyright 2017 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(etcd_client ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(etcd_client
  PUBLIC .
  PUBLIC ${CURL_INCLUDE_DIRS}
)

target_link_libraries(etcd_client
  PUBLIC logging
  PUBLIC ${CURL_LIBRARIES}
  PUBLIC jsoncpp_lib
)
