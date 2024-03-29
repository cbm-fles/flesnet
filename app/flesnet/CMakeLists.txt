# Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.hpp)

list(APPEND SOURCES "${CMAKE_BINARY_DIR}/config/GitRevision.cpp")
list(APPEND HEADERS "${PROJECT_SOURCE_DIR}/config/GitRevision.hpp")

add_executable(flesnet ${SOURCES} ${HEADERS})

target_compile_definitions(flesnet PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(flesnet
  PUBLIC ${PROJECT_SOURCE_DIR}/lib
  PRIVATE "${PROJECT_SOURCE_DIR}/config"
)

target_include_directories(flesnet SYSTEM PUBLIC
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(flesnet
  flib_ipc fles_core fles_ipc shm_ipc fles_zeromq logging monitoring
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
)

if(APPLE)
  target_link_directories(flesnet PRIVATE ${ZSTD_LIB_DIR})
endif()

if (USE_RDMA AND RDMA_FOUND)
  target_compile_definitions(flesnet PUBLIC HAVE_RDMA)
  target_link_libraries(flesnet fles_rdma)
endif()

if (USE_LIBFABRIC AND LIBFABRIC_FOUND)
  target_compile_definitions(flesnet PUBLIC HAVE_LIBFABRIC)
  target_link_libraries(flesnet fles_libfabric)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(flesnet PUBLIC HAVE_PTHREAD_SETNAME_NP)
  target_link_libraries(flesnet rt atomic)
endif()

install(TARGETS flesnet DESTINATION bin)
