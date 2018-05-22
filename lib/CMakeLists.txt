# Copyright 2013-2014, 2016 Jan de Cuveland <cmail@cuveland.de>

add_library(influxdb influxdb.hpp influxdb.cpp)

target_include_directories(influxdb PUBLIC .)
target_include_directories(influxdb SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_include_directories(influxdb PUBLIC ${CPPREST_INCLUDE_DIR})
target_link_libraries(influxdb  
	PUBLIC logging
	PUBLIC ${CPPREST_LIBRARY}
	PUBLIC ${OPENSSL_LIBRARIES}
)


