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

## doxygen warnings/errors that won't fix easily:
#
# 1) 	error: FileDefImpl::insertMembers(): member 'RollingCrc< GenericCrc<
# 		Crc, TableEntry, Word, kStride > >' with class scope '<global>'
# 		inserted in file scope 'generic_crc.h'!
#
# Unclear issue. Possibly a bug in doxygen's lexical parser (compare with
# https://github.com/doxygen/doxygen/issues/9497).
#
# 2)	warning: Detected potential recursive class relation between class
# 		ParametersException and base class std::runtime_error!
#
# This is likely a bug in doxygen's lexical parser related to
# https://github.com/doxygen/doxygen/issues/10165
# i.e. duplicate class names in different files. Possibly introducing
# appropriate namespaces would fix this.
#
# 3) 	warning: documented symbol 'logging::LogBuffer::LogBuffer' was not
# 		declared or defined. (and similar)
#
# Unclear issue.

## Extended documentation:
# Since the documentation currently is not complete or up-to-date,
# enabling the following options makes sense during development. After
# the doxygen comments in the code are improved, which allow for a more
# granular control of the generated documentation, enabling the
# following options will be unnecessary. This will then reduce the
# amount of generated documentation and thus the build time.
if (NOT DEFINED EXTENDED_DOXYGEN_DOCUMENTATION)
  set(EXTENDED_DOXYGEN_DOCUMENTATION NO)
endif()
if (EXTENDED_DOXYGEN_DOCUMENTATION)
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_SHOW_FILES YES)
  set(DOXYGEN_CALL_GRAPH YES)
  set(DOXYGEN_CALLER_GRAPH YES)
  set(DOXYGEN_INLINE_SOURCES YES)
  set(DOXYGEN_GENERATE_TREEVIEW YES)

  # default DOT_GRAPH_MAX_NODES: 50, but too small for this project
  set(DOXYGEN_DOT_GRAPH_MAX_NODES 100)
endif()

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/flesnet-doc)
doxygen_add_docs(doc app/flesnet lib test
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMENT "Generating documentation with doxygen \n\t(doxygen error \
	\"FileDefImpl::insertMembers()...\" is expected)" )
