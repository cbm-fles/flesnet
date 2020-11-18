# Copyright 2013-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_core ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(fles_core PUBLIC .)

target_include_directories(fles_core SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(fles_core
  PUBLIC fles_ipc
  PUBLIC shm_ipc
  PUBLIC logging
  PUBLIC crcutil
)

if(USE_NUMA AND NUMA_FOUND)
  target_compile_definitions(fles_core PRIVATE HAVE_NUMA)
  target_link_libraries(fles_core PRIVATE ${NUMA_LIBRARY})
endif()
