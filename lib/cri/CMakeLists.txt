# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>
# Copyright 2021 Dirk Hutter <cmail@dirk-hutter.de>

set(LIB_SRC
  cri_device.cpp
  cri_channel.cpp
  dma_channel.cpp
  register_file_bar.cpp
)
set(LIB_HDR
  cri_device.hpp
  cri_channel.hpp
  dma_channel.hpp
  register_file.hpp
  register_file_bar.hpp
  data_structures.hpp
  cri.hpp
  cri_registers.hpp
)

Add_library(cri STATIC ${LIB_HDR} ${LIB_SRC})

set_target_properties(cri PROPERTIES OUTPUT_NAME cri)

target_compile_options(cri PUBLIC -fno-tree-vectorize)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   set_source_files_properties(register_file_bar.cpp
        PROPERTIES
        COMPILE_FLAGS "-Wno-unknown-attributes -mno-sse")
endif()

target_include_directories(cri PUBLIC . PUBLIC ..)
target_include_directories(cri SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(cri PUBLIC pda)
