# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
include(ExternalProject)

project(fles)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

IF(NOT DEFINED ENV{SIMPATH})
 MESSAGE(FATAL_ERROR "Path to Fairroot external packages is not set. Please set SIMPATH.")
ENDIF(NOT DEFINED ENV{SIMPATH})
SET(SIMPATH $ENV{SIMPATH})

#set(CMAKE_C_COMPILER clang)
#set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g -Wall -pedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor")

set(Boost_NO_SYSTEM_PATHS TRUE)
set(BOOST_ROOT ${SIMPATH})

find_package(Boost 1.48.0 COMPONENTS serialization thread program_options system REQUIRED)
find_package(Threads)
find_package(ZMQ REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

add_subdirectory(lib/fles_core)
add_subdirectory(lib/flib)
add_subdirectory(lib/flib_dpb)

ExternalProject_add(
 fles_ipc
 PREFIX fles_ipc
 DOWNLOAD_COMMAND ""
 # GIT_SUBMODULES "" not supported in git 2.8
 INSTALL_COMMAND ""
 SOURCE_DIR ${PROJECT_SOURCE_DIR}/lib/fles_ipc
 )

add_subdirectory(app/flesnet)
add_subdirectory(app/tsclient)
add_subdirectory(app/flib_debug)
add_subdirectory(app/flib_ctrl)

add_subdirectory(doc)

add_subdirectory(contrib)

enable_testing()
add_subdirectory(test)
