# Copyright 2016 Farouk Salem <salem@zib.de>

set(LIBFABRIC_PREFIX /home/farouk/workspace/libfabric)

find_path(LIBFABRIC_INCLUDE_DIR rdma/fabric.h)
find_library(LIBFABRIC_LIBRARY NAMES fabric)

set(LIBFABRIC_LIBRARIES ${LIBFABRIC_LIBRARY} )
set(LIBFABRIC_INCLUDE_DIRS ${LIBFABRIC_INCLUDE_DIR} )

MESSAGE(STATUS "libfabric           = ${LIBFABRIC_LIBRARIES}")
MESSAGE(STATUS "libfabric           = ${LIBFABRIC_INCLUDE_DIRS}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBFABRIC REQUIRED_VARS LIBFABRIC_LIBRARIES LIBFABRIC_INCLUDE_DIRS)
