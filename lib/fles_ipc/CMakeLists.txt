# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(fles_ipc)

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g -Wall -pedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor")

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})
