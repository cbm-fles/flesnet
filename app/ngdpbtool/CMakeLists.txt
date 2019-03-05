# Copyright 2015-2016 Jan de Cuveland <cmail@cuveland.de>
# Copyright 2016 Pierre-Alain Loizeau <p.-a.loizeau@gsi.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

list(APPEND APP_SOURCES "${CMAKE_BINARY_DIR}/config/GitRevision.cpp")
list(APPEND APP_HEADERS "${PROJECT_SOURCE_DIR}/config/GitRevision.hpp")

add_executable(ngdpbtool ${APP_SOURCES} ${APP_HEADERS})

target_compile_definitions(ngdpbtool PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(ngdpbtool PRIVATE "${PROJECT_SOURCE_DIR}/config")

target_include_directories(ngdpbtool SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(ngdpbtool
  fles_ipc fles_core flib_ipc logging crcutil fles_tools
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(ngdpbtool rt atomic)
endif()
