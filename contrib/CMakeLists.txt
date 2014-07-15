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

add_custom_target(links ALL DEPENDS run verbs.supp)
