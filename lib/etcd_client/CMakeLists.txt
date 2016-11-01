# Copyright 2016 H.Hartmann <hartmann@compeng.fias.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/logging")

add_library(etcd_client ${LIB_SOURCES} ${LIB_HEADERS})
target_link_libraries(etcd_client jsoncpp curl curlpp)