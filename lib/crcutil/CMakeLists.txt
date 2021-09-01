# Copyright 2015-2016 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cc)
file(GLOB LIB_HEADERS *.h)

if(APPLE)
  # suppress "no symbols" warnings
  set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> cq -S <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols <TARGET>")
endif()

add_compile_options(-Wno-expansion-to-defined -Wno-deprecated-copy)

add_library(crcutil ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(crcutil PUBLIC .)
