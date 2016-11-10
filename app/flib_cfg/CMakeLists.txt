# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(flib_cfg flib_cfg.cpp)

target_compile_definitions(flib_cfg PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(flib_cfg SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(flib_cfg
  flib logging
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt
)
