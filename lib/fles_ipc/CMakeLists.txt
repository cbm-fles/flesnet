# Copyright 2013, 2015 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(fles_ipc)

if(DEFINED ENV{SIMPATH})
  message(WARNING "SIMPATH set, using Fairroot external packages in build.")
  set(Boost_NO_SYSTEM_PATHS TRUE)
  set(BOOST_ROOT $ENV{SIMPATH})
endif(DEFINED ENV{SIMPATH})

find_package(Boost 1.48.0 COMPONENTS serialization unit_test_framework REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g -Wall -pedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor")

file(GLOB LIB_SOURCES src/*.cpp)
file(GLOB LIB_HEADERS src/*.hpp)

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})

enable_testing()
add_subdirectory(test)
