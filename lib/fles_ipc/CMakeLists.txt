# Copyright 2013, 2015, 2016 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})

target_compile_definitions(fles_ipc PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(fles_ipc PUBLIC .)

target_include_directories(fles_ipc SYSTEM
  PUBLIC ${Boost_INCLUDE_DIRS}
  PUBLIC ${ZMQ_INCLUDE_DIRS}
  PUBLIC ${PROJECT_SOURCE_DIR}/external/cppzmq
)

target_link_libraries(fles_ipc PUBLIC ${ZMQ_LIBRARIES} PUBLIC logging )
