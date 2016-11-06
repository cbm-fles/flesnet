# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

find_path(
	PDA_INCLUDE_DIR pda.h
  PATHS /opt/pda/${PDA_FIND_VERSION}/include pda/build_linux_x86_uio/include NO_DEFAULT_PATH)

find_library(
	PDA_LIBRARY pda
  PATHS /opt/pda/${PDA_FIND_VERSION}/lib pda/build_linux_x86_uio NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PDA REQUIRED_VARS PDA_LIBRARY PDA_INCLUDE_DIR)
