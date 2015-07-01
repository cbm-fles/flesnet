cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(LIBFLIB)

include_directories( ${LIBFLIB_SOURCE_DIR}/
                     ${LIBFLIB_SOURCE_DIR}/../)

set(VERSION "0.0.0")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-vectorize -g")

set(LIB_SRC
  dma_channel.cpp
  register_file_bar.cpp
  flib_device.cpp
  flib_device_cnet.cpp
  flib_device_flesin.cpp
  flib_link.cpp
  flib_link_cnet.cpp
  flib_link_flesin.cpp
)
set(LIB_HDR
  flib.h
  dma_channel.hpp
  registers.h
  flib_device.hpp
  flib_device_cnet.hpp
  flib_device_flesin.hpp
  flib_link.hpp
  flib_link_cnet.hpp
  flib_link_flesin.hpp
  register_file.hpp
  register_file_bar.hpp
)

add_library(flib SHARED ${LIB_HDR} ${LIB_SRC})
add_library(flib-static STATIC ${LIB_HDR} ${LIB_SRC})
set_target_properties(flib-static PROPERTIES OUTPUT_NAME flib)

target_link_libraries( flib pda)
target_link_libraries( flib-static pda)
