# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

set(LIB_SRC
  dma_channel.cpp
  register_file_bar.cpp
  flib_device.cpp
  flib_device_cnet.cpp
  flib_device_flesin.cpp
  flib_link.cpp
  flib_link_cnet.cpp
  flib_link_flesin.cpp
  flim.cpp
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
  flim.hpp
  register_file.hpp
  register_file_bar.hpp
)

add_library(flib SHARED ${LIB_HDR} ${LIB_SRC})
add_library(flib-static STATIC ${LIB_HDR} ${LIB_SRC})

set_target_properties(flib-static PROPERTIES OUTPUT_NAME flib)

target_compile_options(flib PUBLIC -fno-tree-vectorize)
target_compile_options(flib-static PUBLIC -fno-tree-vectorize)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   set_source_files_properties(register_file_bar.cpp
        PROPERTIES
        COMPILE_FLAGS "-Wno-unknown-attributes -mno-sse")
endif()

target_include_directories(flib PUBLIC . PUBLIC ..)
target_include_directories(flib SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(flib-static PUBLIC . PUBLIC ..)
target_include_directories(flib-static SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(flib PUBLIC pda)
target_link_libraries(flib-static PUBLIC pda)
