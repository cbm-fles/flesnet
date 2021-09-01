# Copyright 2013-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cxx *.cpp)
file(GLOB LIB_HEADERS *.h *.hpp)

add_library(fles_tools ${LIB_SOURCES} ${LIB_HEADERS})

target_compile_options(fles_tools PUBLIC -Wno-unknown-pragmas -Wno-format-truncation)

target_include_directories(fles_tools PUBLIC .)

target_include_directories(fles_tools SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(fles_tools
  PUBLIC fles_core
  PUBLIC fles_ipc
)
