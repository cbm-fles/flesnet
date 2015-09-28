# Copyright 2014 Dirk Hutter, Jan de Cuveland

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.hpp)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

# Search for external library

# force search if cmake is called
unset(CNETCNTLSERVER CACHE)
unset(CNETCNTLSERVER_INC CACHE)
unset(CNETCNTLSERVER_LIB CACHE)


IF(NOT DEFINED ENV{CNETCNTLSERVER_PATH})
 MESSAGE(WARNING "Path to Control API is not set. Building w/o Controls API support. You may set CNETCNTLSERVER_PATH.")
ELSE(NOT DEFINED ENV{CNETCNTLSERVER_PATH})

message(STATUS "Looking for Control API - seraching in $ENV{CNETCNTLSERVER_PATH}")

find_path(CNETCNTLSERVER_INC NAMES control/libserver/ControlServer.hpp
             PATHS ENV CNETCNTLSERVER_PATH)

find_library(CNETCNTLSERVER_LIB NAMES libcbmnetcntlserver.so
             PATHS ENV CNETCNTLSERVER_PATH)

if(CNETCNTLSERVER_LIB AND CNETCNTLSERVER_INC)
  SET(CNETCNTLSERVER TRUE)
endif(CNETCNTLSERVER_LIB AND CNETCNTLSERVER_INC)

if(CNETCNTLSERVER)
message(STATUS "Looking for Control API - found in ${CNETCNTLSERVER_LIB};${CNETCNTLSERVER_INC}")
else(CNETCNTLSERVER)
message(WARNING "Looking for Control API - not found, building w/o Controls API support")
endif(CNETCNTLSERVER)

ENDIF(NOT DEFINED ENV{CNETCNTLSERVER_PATH})


include_directories("${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/flib"
                    "${PROJECT_SOURCE_DIR}/lib"
                    "${PROJECT_SOURCE_DIR}/lib/flib_ctrl_server")

add_executable(flib_ctrl flib_ctrl.cpp parameters.hpp)

target_link_libraries(flib_ctrl flib logging ${ZMQ_LIBRARIES} ${Boost_LIBRARIES} rt)

if(CNETCNTLSERVER)
    target_link_libraries(flib_ctrl ${CNETCNTLSERVER_LIB})
    include_directories("${CNETCNTLSERVER_INC}")
    set_property(   TARGET      flib_ctrl
                    PROPERTY    COMPILE_DEFINITIONS CNETCNTLSERVER)
endif(CNETCNTLSERVER)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(flib_ctrl pthread)
endif()

