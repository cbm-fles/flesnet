# Copyright 2025 Jan de Cuveland

add_executable(tsbuild
  main.cpp
  Application.cpp
  Parameters.cpp
  TsBuffer.cpp
  TsBuilder.cpp
)

target_include_directories(tsbuild
  SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}
)

target_include_directories(tsbuild
  PUBLIC ../../lib
)

target_link_libraries(tsbuild
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

install(TARGETS tsbuild DESTINATION bin)
