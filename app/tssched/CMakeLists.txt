# Copyright 2025 Jan de Cuveland

add_executable(tssched
  main.cpp
  Application.cpp
  Parameters.cpp
  TsScheduler.cpp
)

target_include_directories(tssched
  SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}
)

target_include_directories(tssched
  PUBLIC ../../lib
)

target_link_libraries(tssched
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

install(TARGETS tssched DESTINATION bin)
