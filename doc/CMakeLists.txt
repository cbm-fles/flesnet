# Copyright 2013,2016 Jan de Cuveland <cmail@cuveland.de>

## SETTING DOXYGEN CONFIGURATION OPTIONS
#
# Configuring doxygen is best done by setting the corresponding cmake
# variables here (see FindDoxygen in the cmake documentation). A
# Doxyfile.in is then generated. Providing a custom Doxyfile.in is
# possible but unnecessary. 
#
# A doxygen option <tag> is set by setting the variable DOXYGEN_<tag> to
# the desired value. For a complete list of options see the doxygen
# manual. Note, that cmake changes the default value of some options as
# documented in the cmake manual. For example, the default value of
# DOXYGEN_RECURSIVE is changed to YES.

set(DOXYGEN_LANGUAGE "English" CACHE STRING "Language used by doxygen")
mark_as_advanced(DOXYGEN_LANGUAGE)
set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Non-default doxygen configuration options from the obsolete
# Doxyfile.in:
set(DOXYGEN_TAB_SIZE 8)
set(DOXYGEN_BUILTIN_STL_SUPPORT YES)

# Silence doxygen:
if (NOT DEFINED SILENCE_DOXYGEN)
  set(SILENCE_DOXYGEN YES)
endif()

if (SILENCE_DOXYGEN)
  set(DOXYGEN_QUIET YES)
  set(DOXYGEN_WARNINGS NO)
  set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
  set(DOXYGEN_WARN_IF_DOC_ERROR NO)
  set(DOXYGEN_WARN_IF_INCOMPLETE_DOC NO)
else()
  # Even if doxygen is not supposed to be silent, the sheer amount of
  # undocumented code in this project is overwhelming. Thus, silence
  # doxygen warnings about undocumented code anyway.
  if (NOT DEFINED DOXYGEN_WARN_IF_UNDOCUMENTED)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
  endif()
endif()

# Fix warnings resulting from an attempt to build a glossary:
set(DOXYGEN_ALIASES "glos{1}=\\1")

doxygen_add_docs(doc ../app ../lib ../test)
