# Copyright 2016 H.Hartmann <hartmann@compeng.fias.uni-frankfurt.de>


add_library(etcdClient etcdClient.cpp etcdClient.h)
target_link_libraries(etcdClient curl curlpp jsoncpp)
