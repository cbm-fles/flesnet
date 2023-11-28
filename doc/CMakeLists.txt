# Copyright 2013,2016 Jan de Cuveland <cmail@cuveland.de>

set(DOXYGEN_LANGUAGE "English" CACHE STRING "Language used by doxygen")
mark_as_advanced(DOXYGEN_LANGUAGE)

set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

doxygen_add_docs(doc ../app ../lib ../test )
