# Copyright 2025 Jan de Cuveland <cmail@cuveland.de>

add_executable(ucx_demo ucx_demo.cpp)

target_link_libraries(ucx_demo
  fles_ucx
  ${CMAKE_THREAD_LIBS_INIT}
)
