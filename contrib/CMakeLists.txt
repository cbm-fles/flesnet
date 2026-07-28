# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#                Dirk Hutter <hutter@compeng.uni-frankfurt.de>

add_custom_command(
  OUTPUT boost.supp
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/boost.supp ${CMAKE_BINARY_DIR}/boost.supp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/boost.supp)

add_custom_target (
	cfg-files ALL
	COMMAND ${CMAKE_COMMAND}
          -DCP_SRC:STRING=${CMAKE_CURRENT_SOURCE_DIR}/cri.cfg
          -DCP_DEST:STRING=${CMAKE_BINARY_DIR}/cri.cfg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CopyIfNotExits.cmake
)

add_custom_target(links ALL DEPENDS boost.supp)

install(PROGRAMS preclean DESTINATION bin)
