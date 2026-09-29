# Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
# SPDX-License-Identifier: GPL-3.0-only
# Author: Jan de Cuveland */

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(tsb_ucx ${LIB_SOURCES} ${LIB_HEADERS})

# Uses std::format and other C++23 features, independent of the global default
target_compile_features(tsb_ucx PUBLIC cxx_std_23)

target_include_directories(tsb_ucx PUBLIC .)

target_include_directories(tsb_ucx SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(tsb_ucx
  PUBLIC fles_ipc
  PUBLIC logging
  PUBLIC ucx::ucp
  PUBLIC ucx::uct
  PUBLIC ucx::ucs
)
