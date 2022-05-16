# Copyright 2013-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

if(NOT USE_CPPREST OR NOT CPPREST_FOUND)
  list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/TimesliceAnalyzer.cpp)
  list(REMOVE_ITEM LIB_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/TimesliceAnalyzer.hpp)
endif()

add_library(fles_core ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(fles_core PUBLIC .)

target_include_directories(fles_core SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

if(USE_CPPREST AND CPPREST_FOUND)
  target_include_directories(fles_core SYSTEM PUBLIC ${CPPREST_INCLUDE_DIR})
endif()

target_link_libraries(fles_core
  PUBLIC fles_ipc
  PUBLIC shm_ipc
  PUBLIC logging
  PUBLIC crcutil
)

if(USE_CPPREST AND CPPREST_FOUND)
  target_link_libraries(fles_core PUBLIC ${CPPREST_LIBRARY} crypto)
endif()

if(USE_NUMA AND NUMA_FOUND)
  target_compile_definitions(fles_core PRIVATE HAVE_NUMA)
  target_link_libraries(fles_core PRIVATE ${NUMA_LIBRARY})
endif()
