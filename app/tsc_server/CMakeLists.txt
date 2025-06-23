# Copyright 2025 Dirk Hutter, Jan de Cuveland

add_executable(tsc_server
  main.cpp
  Application.cpp
  Parameters.cpp
  Channel.cpp
  StSender.cpp
  ucxutil.cpp
)

target_include_directories(tsc_server
  SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(tsc_server
  pda
  cri
  fles_ipc
  fles_core
  logging
  ucx::ucp
  ucx::uct
  ucx::ucs
  ${Boost_LIBRARIES}
)

install(TARGETS tsc_server DESTINATION bin)
