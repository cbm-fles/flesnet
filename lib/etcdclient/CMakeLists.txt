# Copyright 2016 H.Hartmann <hartmann@compeng.fias.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/httpClient"
		    "${PROJECT_SOURCE_DIR}/lib/logging")

add_library(etcdClient ${LIB_SOURCES} ${LIB_HEADERS})
target_link_libraries(etcdClient jsoncpp httpClient)
