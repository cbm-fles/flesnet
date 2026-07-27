# Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
# SPDX-License-Identifier: GPL-3.0-only
# Author: Jan de Cuveland

add_executable(tsmanager
  main.cpp
  Application.cpp
  Parameters.cpp
  TsManager.cpp
)

target_include_directories(tsmanager
  SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}
)

target_include_directories(tsmanager
  PUBLIC ../../lib
)

target_link_libraries(tsmanager
  fles_ipc
  fles_core
  tsb
  logging
  monitoring
  ucx::ucp
  ucx::uct
  ucx::ucs
  ${Boost_LIBRARIES}
)

install(TARGETS tsmanager DESTINATION bin)
