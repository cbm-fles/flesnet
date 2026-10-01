# Copyright 2013-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_tools ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(fles_tools PUBLIC .)

target_include_directories(fles_tools SYSTEM PUBLIC ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

target_link_libraries(fles_tools
  PUBLIC fles_ipc
  PUBLIC shm_ipc
  PUBLIC logging
  PUBLIC crcutil
  PUBLIC monitoring
)

if(USE_NUMA AND NUMA_FOUND)
  target_compile_definitions(fles_tools PRIVATE HAVE_NUMA)
  target_link_libraries(fles_tools PRIVATE ${NUMA_LIBRARY})
endif()
