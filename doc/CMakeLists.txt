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
doxygen_add_docs(doc-flesnet
  app/flesnet
  lib/logging
  lib/shm_ipc
  lib/fles_core
  lib/fles_zeromq
  lib/fles_ipc
  lib/monitoring
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMENT "Generating flesnet documentation with doxygen")

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mstool-doc)
doxygen_add_docs(doc-mstool
  app/mstool
  lib/logging
  lib/fles_core
  lib/fles_ipc
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating documentation with doxygen")

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tsclient-doc)
doxygen_add_docs(doc-tsclient
  app/tsclient
  lib/logging
  lib/fles_core
  lib/fles_ipc
  lib/monitoring
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating documentation wit doxygen" )

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/msconsumer-doc)
doxygen_add_docs(doc-msconsumer app/msconsumer lib test
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating documentation with doxygen \n\t(doxygen error \
  \"FileDefImpl::insertMembers()...\" is expected)")

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cri_cfg-doc)
doxygen_add_docs(doc-cri_cfg
  app/cri_cfg
  lib/cri
  lib/fles_ipc/MicrosliceDescriptor.hpp
  lib/pda
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating documentation with doxygen")

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cri_server-doc)
doxygen_add_docs(doc-cri_server
  app/cri_server
  lib/logging
  lib/fles_ipc/MicrosliceDescriptor.hpp
  lib/fles_ipc/Utility.hpp
  lib/cri
  lib/pda
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating documentation with doxygen" )

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cri_tools-doc/cri_en_pgen-doc)
doxygen_add_docs(doc-cri_en_pgen
  app/cri_tools/cri_en_pgen.cpp
  lib/cri
  lib/pda
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating cri_en_pgen documentation with doxygen")

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cri_tools-doc/cri_info-doc)
doxygen_add_docs(doc-cri_info
  app/cri_tools/cri_info.cpp
  lib/fles_ipc/MicrosliceDescriptor.hpp
  lib/cri
  lib/pda
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating cri_info documentation with doxygen")
  
set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cri_tools-doc/cri_status-doc)
doxygen_add_docs(doc-cri_status
  app/cri_tools/cri_status.cpp
  lib/monitoring
  lib/cri
  lib/fles_ipc/System.hpp
  lib/fles_ipc/MicrosliceDescriptor.hpp
  lib/pda
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating cri_status documentation with doxygen")

set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cri_tools-doc/cri_test_rf-doc)
doxygen_add_docs(doc-cri_test_rf
  app/cri_tools/cri_test_rf.cpp
  lib/fles_ipc/MicrosliceDescriptor.hpp
  lib/cri
  lib/pda
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating cri_test_rf documentation with doxygen")

add_custom_target(doc-cri_tools
  DEPENDS
    doc-cri_info
    doc-cri_en_pgen
    doc-cri_status
    doc-cri_test_rf)

add_custom_target(doc
  DEPENDS
    doc-flesnet
    doc-mstool
    doc-tsclient
    doc-msconsumer
    doc-cri_cfg
    doc-cri_server
    doc-cri_tools)
