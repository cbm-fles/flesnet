# Copyright 2025 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(ucxutil ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(ucxutil PUBLIC .)

target_link_libraries(ucxutil
  PUBLIC logging
  PUBLIC ucx::ucp
  PUBLIC ucx::uct
  PUBLIC ucx::ucs
)
