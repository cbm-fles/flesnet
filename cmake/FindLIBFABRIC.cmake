# Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

find_path(LIBFABRIC_INCLUDE_DIR rdma/fabric.h)
find_library(LIBFABRIC_LIBRARY NAMES fabric)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBFABRIC REQUIRED_VARS LIBFABRIC_LIBRARY LIBFABRIC_INCLUDE_DIR)
