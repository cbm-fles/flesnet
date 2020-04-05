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

add_custom_command(
  OUTPUT boost.supp
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/boost.supp ${CMAKE_BINARY_DIR}/boost.supp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/boost.supp)

add_custom_command(
  OUTPUT shm_mstool
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/shm_mstool ${CMAKE_BINARY_DIR}/shm_mstool
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shm_mstool)

add_custom_command(
  OUTPUT shm_flesnet
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/shm_flesnet ${CMAKE_BINARY_DIR}/shm_flesnet
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shm_flesnet)

add_custom_target (
	cfg-files ALL
	COMMAND ${CMAKE_COMMAND}
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/flib_cfg_all
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/flib_cfg_all
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
	COMMAND ${CMAKE_COMMAND}
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/flib.cfg
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/flib.cfg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
	COMMAND ${CMAKE_COMMAND}
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/flib_server.cfg
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/flib_server.cfg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
	COMMAND ${CMAKE_COMMAND}
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/flesnet_simple_example.cfg
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/flesnet.cfg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
	COMMAND ${CMAKE_COMMAND}
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/readout
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/readout
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
	COMMAND ${CMAKE_COMMAND}
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/record_data.in
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/record_data
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
)

add_custom_target(links ALL DEPENDS run verbs.supp boost.supp shm_mstool shm_flesnet)

install(PROGRAMS preclean DESTINATION bin)
