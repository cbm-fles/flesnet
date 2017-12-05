# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(flib_server flib_server.cpp)
add_executable(simple_consumer simple_consumer.cpp)

target_compile_definitions(flib_server
  PRIVATE BOOST_INTERPROCESS_ENABLE_TIMEOUT_WHEN_LOCKING
  PRIVATE BOOST_INTERPROCESS_TIMEOUT_WHEN_LOCKING_DURATION_MS=5000
  PUBLIC BOOST_ALL_DYN_LINK
)
target_compile_definitions(simple_consumer PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(flib_server SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(flib_server
  flib flib_ipc fles_ipc fles_core logging
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt
)
target_link_libraries(simple_consumer
  flib_ipc logging
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt
)
