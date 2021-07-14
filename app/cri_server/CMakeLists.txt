# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(cri_server cri_server.cpp)

target_compile_definitions(cri_server
  PRIVATE BOOST_INTERPROCESS_ENABLE_TIMEOUT_WHEN_LOCKING
  PRIVATE BOOST_INTERPROCESS_TIMEOUT_WHEN_LOCKING_DURATION_MS=5000
  PUBLIC BOOST_ALL_DYN_LINK
)

target_include_directories(cri_server SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(cri_server
  cri flib_ipc fles_ipc fles_core logging
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt
)

install(TARGETS cri_server DESTINATION bin)
