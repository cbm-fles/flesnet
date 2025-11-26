# Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
# SPDX-License-Identifier: GPL-3.0-only
# Authors: Dirk Hutter, Jan de Cuveland

add_executable(stserver
  main.cpp
  Application.cpp
  Parameters.cpp
  Channel.cpp
  StSender.cpp
  pgen_channel.cpp
)

target_include_directories(stserver
  SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(stserver
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

install(TARGETS stserver DESTINATION bin)
