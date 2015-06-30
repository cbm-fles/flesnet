# Copyright 2013-2015 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(fles)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if(DEFINED ENV{SIMPATH})
  message(WARNING "SIMPATH set, using Fairroot external packages in build.")
  set(Boost_NO_SYSTEM_PATHS TRUE)
  set(BOOST_ROOT $ENV{SIMPATH})
  set(SIMPATH $ENV{SIMPATH})
  find_package(ZMQ REQUIRED)
else()
  find_library(ZMQ_LIBRARIES zmq)
endif()

#set(CMAKE_C_COMPILER clang)
#set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -ggdb -Wall -pedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor -fno-omit-frame-pointer")

find_package(Boost 1.54.0 REQUIRED COMPONENTS log serialization thread program_options system unit_test_framework)
find_package(Threads)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

add_definitions(-DBOOST_LOG_DYN_LINK)

add_subdirectory(lib/logging)
add_subdirectory(lib/fles_ipc)

add_subdirectory(app/tsclient)

add_subdirectory(doc)

if(UNIX AND NOT APPLE)
  find_package(PDA 10.2.6 REQUIRED)

  add_subdirectory(lib/fles_core)
  add_subdirectory(lib/flib)
  add_subdirectory(lib/pda)

  add_subdirectory(app/flesnet)
  add_subdirectory(app/flib_debug)
  add_subdirectory(app/flib_tools)
  add_subdirectory(app/flib_ctrl)
  add_subdirectory(app/flib_cfg)
  add_subdirectory(app/flib_server)

  add_subdirectory(contrib)
endif()
