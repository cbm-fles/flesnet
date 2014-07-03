# Copyright 2014 Dirk Hutter, Jan de Cuveland

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3 -Wstack-usage=2048")

set(LIB_SRC
  src/rorcfs_bar.cpp
  src/rorcfs_buffer.cpp
  src/rorcfs_device.cpp
  src/rorcfs_dma_channel.cpp
)
set(LIB_HDR
  include/flib.h
  include/flib/rorcfs_bar.hh
  include/flib/rorcfs_buffer.hh
  include/flib/rorcfs_device.hh
  include/flib/rorcfs_dma_channel.hh
  include/flib/rorcfs.h
  include/flib/rorc_registers.h
  include/flib/flib_device.hpp
  include/flib/flib_link.hpp
  include/flib/register_file.hpp
  include/flib/register_file_bar.hpp
)

add_library(flib SHARED ${LIB_HDR} ${LIB_SRC})
add_library(flib-static STATIC ${LIB_HDR} ${LIB_SRC})
set_target_properties(flib-static PROPERTIES OUTPUT_NAME flib)

install(TARGETS flib flib-static
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
