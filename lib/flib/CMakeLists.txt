cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(LIBFLIB)

include_directories( ${LIBFLIB_SOURCE_DIR}/ )

set(VERSION "0.0.0")

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3 -Wstack-usage=2048 -fno-tree-vectorize")

set(LIB_SRC
  pci_bar.cpp
  dma_buffer.cpp
  device.cpp
  dma_channel.cpp
  register_file_bar.cpp
  flib_device.cpp
  flib_link.cpp
)
set(LIB_HDR
  flib.h
  pci_bar.hpp
  dma_buffer.hpp
  device.hpp
  dma_channel.hpp
  registers.h
  flib_device.hpp
  flib_link.hpp
  register_file.hpp
  register_file_bar.hpp
)

add_library(flib SHARED ${LIB_HDR} ${LIB_SRC})
add_library(flib-static STATIC ${LIB_HDR} ${LIB_SRC})
set_target_properties(flib-static PROPERTIES OUTPUT_NAME flib)

target_link_libraries( flib ${EXTRA_LIBS})
target_link_libraries( flib-static ${EXTRA_LIBS})
