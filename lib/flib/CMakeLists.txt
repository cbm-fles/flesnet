cmake_minimum_required(VERSION 2.8.9 FATAL_ERROR)
project(LIBFLIB)

include_directories( ${LIBFLIB_SOURCE_DIR}/include )

set( REQUIRED_PDA_VERSION "8:0:3" )
set(VERSION "0.0.0")

include(CMakePda.txt)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3 -Wstack-usage=2048 -fno-tree-vectorize")

set(LIB_SRC
  src/pci_bar.cpp
  src/dma_buffer.cpp
  src/device.cpp
  src/dma_channel.cpp
  src/register_file_bar.cpp
  src/flib_device.cpp
  src/flib_link.cpp
)
set(LIB_HDR
  include/flib.h
  include/flib/pci_bar.hpp
  include/flib/dma_buffer.hpp
  include/flib/device.hpp
  include/flib/dma_channel.hpp
  include/flib/registers.h
  include/flib/flib_device.hpp
  include/flib/flib_link.hpp
  include/flib/register_file.hpp
  include/flib/register_file_bar.hpp
)

add_library(flib SHARED ${LIB_HDR} ${LIB_SRC})
add_library(flib-static STATIC ${LIB_HDR} ${LIB_SRC})
set_target_properties(flib-static PROPERTIES OUTPUT_NAME flib)

target_link_libraries( flib ${EXTRA_LIBS})
target_link_libraries( flib-static ${EXTRA_LIBS})

install(TARGETS flib flib-static
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
