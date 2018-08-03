# Copyright 2017-2018, Julian Reinhardt <jreinhardt@compeng.uni-frankfurt.de>

add_library(influxdb influxdb.hpp influxdb.cpp)

target_include_directories(influxdb PUBLIC .)
target_include_directories(influxdb SYSTEM PUBLIC 
	${Boost_INCLUDE_DIRS}
	${CPPREST_INCLUDE_DIR}
)

target_link_libraries(influxdb  
	PUBLIC logging
	PUBLIC ${CPPREST_LIBRARY}
	PUBLIC ${OPENSSL_LIBRARIES}
)


