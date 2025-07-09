# SPDX-License-Identifier: GPL-3.0-only
# (C) Copyright 2022 Johann Wolfgang Goethe-Universität Frankfurt
# Original author: Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)
file(GLOB LIB_IPP *.ipp)

add_library(monitoring ${LIB_SOURCES} ${LIB_IPP} ${LIB_HEADERS})

target_include_directories(monitoring PUBLIC .)

target_link_libraries(monitoring
  PRIVATE Boost::boost
  PUBLIC Threads::Threads
  PUBLIC fmt::fmt
)

target_compile_features(monitoring PUBLIC cxx_std_17)

target_compile_options(monitoring PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(monitoring PUBLIC HAVE_PTHREAD_SETNAME_NP)
endif()
