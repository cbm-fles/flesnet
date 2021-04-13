# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>
# Copyright 2021 Dirk Hutter <cmail@dirk-hutter.de>

set(LIB_SRC
  register_file_bar.cpp
  cri_device.cpp
)
set(LIB_HDR
  cri.hpp
  cri_registers.hpp
  cri_device.hpp
  register_file.hpp
  register_file_bar.hpp
  data_structures.hpp  
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
