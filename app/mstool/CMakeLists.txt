# Copyright 2015-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

list(APPEND APP_SOURCES "${CMAKE_BINARY_DIR}/config/GitRevision.cpp")
list(APPEND APP_HEADERS "${PROJECT_SOURCE_DIR}/config/GitRevision.hpp")

add_executable(mstool ${APP_SOURCES} ${APP_HEADERS})

target_compile_definitions(mstool PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(mstool PRIVATE "${PROJECT_SOURCE_DIR}/config")

target_include_directories(mstool SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(mstool
  fles_ipc fles_core flib_ipc logging crcutil
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${ZMQ_LIBRARIES}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(mstool rt atomic)
endif()
