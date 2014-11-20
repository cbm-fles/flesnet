# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#                Dirk Hutter <hutter@compeng.uni-frankfurt.de>

add_custom_command(
  OUTPUT run
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/run ${CMAKE_BINARY_DIR}/run
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/run)

add_custom_command(
  OUTPUT verbs.supp
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/verbs.supp ${CMAKE_BINARY_DIR}/verbs.supp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/verbs.supp)


add_custom_target (
	cfg-files ALL
	COMMAND ${CMAKE_COMMAND} 
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/flib.cfg 
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/flib.cfg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
	COMMAND ${CMAKE_COMMAND} 
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/flesnet_flib_example.cfg 
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/flesnet.cfg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
	COMMAND ${CMAKE_COMMAND} 
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/plesnet 
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/plesnet
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
)

add_custom_target(links ALL DEPENDS run verbs.supp)
