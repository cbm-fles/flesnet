# Copyright 2016 H.Hartmann <hartmann@compeng.fias.uni-frankfurt.de>

include_directories("${PROJECT_SOURCE_DIR}/lib/logging")

add_library(httpClient httpClient.cpp httpClient.hpp)
target_link_libraries(httpClient curl curlpp jsoncpp)
