# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

include_directories("${PROJECT_SOURCE_DIR}/app/flesnet" "${PROJECT_SOURCE_DIR}/lib/einhard")

add_library(fles_core ${LIB_SOURCES} ${LIB_HEADERS})
