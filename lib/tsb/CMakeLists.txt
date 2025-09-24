# Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
# SPDX-License-Identifier: GPL-3.0-only
# Author: Jan de Cuveland */

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(tsb ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(tsb PUBLIC .)

target_include_directories(fles_core SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(tsb
  PUBLIC fles_ipc
  PUBLIC logging
  PUBLIC ucx::ucp
  PUBLIC ucx::uct
  PUBLIC ucx::ucs
)
