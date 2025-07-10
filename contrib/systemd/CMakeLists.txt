# Copyright 2025 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

include(GNUInstallDirs) # set CMAKE_INSTALL_FULL_BINDIR

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cri_status.service.in
  ${CMAKE_CURRENT_BINARY_DIR}/cri_status.service
  @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cri_status.service DESTINATION lib/systemd/system)
