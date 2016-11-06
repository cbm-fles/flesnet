cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(LIBPDA)

include_directories(${LIBPDA_SOURCE_DIR} ${PDA_INCLUDE_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-vectorize")

set(LIB_SRC
  pci_bar.cpp
  dma_buffer.cpp
  device.cpp
  device_operator.cpp
)
set(LIB_HDR
  pci_bar.hpp
  dma_buffer.hpp
  device.hpp
  device_operator.hpp
)

add_library(pda ${LIB_HDR} ${LIB_SRC})
set_target_properties(pda PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(pda ${PDA_LIBRARY})
