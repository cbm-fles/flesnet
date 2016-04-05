# Copyright 2013-2016 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(fles)

option(COVERALLS "Turn on coveralls support" OFF)
option(COVERALLS_UPLOAD "Upload the generated coveralls json" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(GetGitRevisionDescription)
git_describe(GIT_REVISION "--all" "--tags" "--long" "--always")
configure_file("${PROJECT_SOURCE_DIR}/config/GitRevision.cpp.in"
               "${CMAKE_BINARY_DIR}/config/GitRevision.cpp" @ONLY)

if(DEFINED ENV{SIMPATH})
  message(WARNING "SIMPATH set, using Fairroot external packages in build.")
  set(Boost_NO_SYSTEM_PATHS TRUE)
  set(BOOST_ROOT $ENV{SIMPATH})
  set(SIMPATH $ENV{SIMPATH})
  find_package(ExternalZMQ REQUIRED)
else()
  find_library(ZMQ_LIBRARIES zmq)
endif()

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -ggdb -msse4.2 -Wall -Wpedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor -fno-omit-frame-pointer")

find_library(LIBNUMA numa)
set(USE_LIBNUMA TRUE CACHE BOOL "Use libnuma to schedule on specific NUMA nodes.")
if(LIBNUMA AND USE_LIBNUMA)
  add_definitions(-DHAVE_LIBNUMA)
endif()
if(USE_LIBNUMA AND NOT LIBNUMA)
  message(STATUS "Library not found: libnuma. Building without.")
endif()

find_package(Boost 1.54.0 REQUIRED COMPONENTS filesystem log program_options serialization system thread unit_test_framework regex)
find_package(Threads)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS} ${ZMQ_INCLUDE_DIRS})

add_definitions(-DBOOST_ALL_DYN_LINK)

if (COVERALLS)
  include(Coveralls)
  coveralls_turn_on_coverage()
endif()

add_subdirectory(lib/logging)
add_subdirectory(lib/crcutil)
add_subdirectory(lib/fles_ipc)
add_subdirectory(lib/fles_core)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(app/tsclient)
add_subdirectory(app/mstool)
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)

add_subdirectory(doc)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(PDA 11.1.7 REQUIRED)

  add_subdirectory(lib/fles_rdma)
  add_subdirectory(lib/flib)
  add_subdirectory(lib/pda)

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  add_subdirectory(app/flesnet)
  add_subdirectory(app/flib_tools)
  add_subdirectory(app/flib_cfg)
  add_subdirectory(app/flib_server)
  unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)

  add_subdirectory(contrib)
endif()

if (COVERALLS)
  file(GLOB_RECURSE LIB_SRC lib/*.[ch]pp)
  coveralls_setup("${LIB_SRC}" ${COVERALLS_UPLOAD})
endif()

enable_testing()
add_subdirectory(test)
