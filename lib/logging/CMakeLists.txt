add_library(logging log.cpp log.hpp)

target_compile_definitions(logging
  PUBLIC BOOST_LOG_DYN_LINK
  PUBLIC BOOST_LOG_USE_NATIVE_SYSLOG
)

target_include_directories(logging PUBLIC .)

target_include_directories(logging SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(logging
  PUBLIC ${Boost_LIBRARIES}
  PUBLIC ${CMAKE_THREAD_LIBS_INIT}
)