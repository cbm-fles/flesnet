cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(LIBPDA)

include_directories( ${LIBPDA_SOURCE_DIR}/ )

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3 -Wstack-usage=2048 -fno-tree-vectorize")

set(LIB_SRC
  pci_bar.cpp
  dma_buffer.cpp
  device.cpp
)
set(LIB_HDR
  pci_bar.hpp
  dma_buffer.hpp
  device.hpp
)

add_library(pda ${LIB_HDR} ${LIB_SRC})
set_target_properties(pda PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(pda ${EXTRA_LIBS})
