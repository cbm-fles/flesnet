# Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
# SPDX-License-Identifier: GPL-3.0-only
# Authors: Dirk Hutter, Jan de Cuveland

add_executable(stserver
  main.cpp
  Application.cpp
  Parameters.cpp
  Channel.cpp
  StBuilder.cpp
  StSender.cpp
  sw_dma_channel.cpp
  pgen_channel.cpp
  replay_channel.cpp
)

target_include_directories(stserver
  SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(stserver
  pda
  cri
  fles_tools
  tsb_ucx
  logging
  git_revision
  ucx::ucp
  ucx::uct
  ucx::ucs
  ${Boost_LIBRARIES}
)

install(TARGETS stserver DESTINATION bin)
