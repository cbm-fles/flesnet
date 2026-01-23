# Copyright 2013-2014, 2016 Jan de Cuveland <cmail@cuveland.de>

add_library(logging BoostHelper.cpp BoostHelper.hpp log.cpp log.hpp)

target_compile_definitions(logging
  PUBLIC BOOST_LOG_DYN_LINK
  PUBLIC BOOST_LOG_USE_NATIVE_SYSLOG
)

target_include_directories(logging PUBLIC .)

target_include_directories(logging SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(logging
  PUBLIC ${Boost_LOG_LIBRARY}
  PUBLIC ${Boost_THREAD_LIBRARY}
  PUBLIC ${Boost_SYSTEM_LIBRARY}
)
