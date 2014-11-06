# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(fles_ipc)

IF(NOT DEFINED ENV{SIMPATH})
 MESSAGE(FATAL_ERROR "Path to Fairroot external packages is not set. Please set SIMPATH.")
ENDIF(NOT DEFINED ENV{SIMPATH})
SET(SIMPATH $ENV{SIMPATH})

set(Boost_NO_SYSTEM_PATHS TRUE)
set(BOOST_ROOT ${SIMPATH})

find_package(Boost 1.48.0 COMPONENTS serialization thread program_options system REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g -Wall -pedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor")

file(GLOB LIB_SOURCES src/*.cpp)
file(GLOB LIB_HEADERS src/*.hpp)

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})

enable_testing()
add_subdirectory(test)
