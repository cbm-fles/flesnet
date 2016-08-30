# Copyright 2013-2014 Jan de Cuveland <cmail@cuveland.de>

add_definitions(-DBOOST_LOG_DYN_LINK)
add_definitions(-DBOOST_LOG_USE_NATIVE_SYSLOG)

add_library(logging log.cpp log.hpp)
