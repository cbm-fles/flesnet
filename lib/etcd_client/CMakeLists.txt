# Copyright 2016 H.Hartmann <hartmann@compeng.fias.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/logging")

add_library(etcdclient ${LIB_SOURCES} ${LIB_HEADERS})
add_library(httpclient ${LIB_SOURCES} ${LIB_HEADERS})
target_link_libraries(httpclient curl curlpp jsoncpp)
target_link_libraries(etcdclient jsoncpp httpclient)
