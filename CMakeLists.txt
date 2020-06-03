# Copyright 2020 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(ipc_demo)

find_library(ZMQ_LIBRARIES zmq)

add_compile_options(-std=c++17 -O3 -ggdb -msse4.2 -Wall -Wpedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor -fno-omit-frame-pointer)

add_subdirectory(zmq_demo)
