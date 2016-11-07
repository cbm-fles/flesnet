# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

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

target_compile_options(pda PRIVATE -fno-tree-vectorize)

target_include_directories(pda
  PUBLIC .
  PUBLIC ${PDA_INCLUDE_DIR}
)

target_link_libraries(pda
  PUBLIC ${PDA_LIBRARY}
)
