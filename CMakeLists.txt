# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(flesnet)

#set(CMAKE_C_COMPILER clang)
#set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g -Wall -pedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor")

set(Boost_NO_SYSTEM_PATHS TRUE)
set(BOOST_ROOT "/opt/fairsoft/oct13_build")

find_package(Boost 1.48.0 COMPONENTS serialization thread program_options system REQUIRED)
find_package(Threads)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

add_subdirectory(lib/fles_core)
add_subdirectory(lib/fles_ipc)

add_subdirectory(app/flesnet)
add_subdirectory(app/ipc-demo)

add_subdirectory(doc)

add_subdirectory(contrib)
