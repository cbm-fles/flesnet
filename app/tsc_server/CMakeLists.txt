# Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
# SPDX-License-Identifier: GPL-3.0-only
# Authors: Dirk Hutter, Jan de Cuveland

add_executable(tsc_server
  main.cpp
  Application.cpp
  Parameters.cpp
  Channel.cpp
  StSender.cpp
  pgen_channel.cpp
)

target_include_directories(tsc_server
  SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(tsc_server
  pda
  cri
  fles_core
  tsb
  logging
  ucx::ucp
  ucx::uct
  ucx::ucs
  ${Boost_LIBRARIES}
)

install(TARGETS tsc_server DESTINATION bin)
