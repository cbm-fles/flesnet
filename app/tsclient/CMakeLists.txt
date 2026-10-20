# Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

add_executable(tsclient ${APP_SOURCES} ${APP_HEADERS})

# For OptionValues.hpp and TimesliceShmSink.hpp
target_compile_features(tsclient PRIVATE cxx_std_23)

target_compile_definitions(tsclient PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(tsclient SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(tsclient
  fles_ipc fles_tools logging crcutil monitoring git_revision
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
)

if(APPLE)
  target_link_directories(tsclient PRIVATE ${ZSTD_LIB_DIR})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(tsclient rt)
endif()

install(TARGETS tsclient DESTINATION bin)
