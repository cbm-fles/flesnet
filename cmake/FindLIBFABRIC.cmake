# Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

find_path(LIBFABRIC_INCLUDE_DIR rdma/fabric.h)
find_library(LIBFABRIC_LIBRARY NAMES fabric)

find_path(ZMQ_INCLUDE_DIR include/zmq.h)
find_library(ZMQ_LIBRARY NAMES libzmq)

set(LIBFABRIC_LIBRARIES ${LIBFABRIC_LIBRARY} )
set(LIBFABRIC_INCLUDE_DIRS ${LIBFABRIC_INCLUDE_DIR} )

set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR} )
set(ZMQ_LIBRARIES ${ZMQ_LIBRARY} )

MESSAGE(STATUS "libfabric           = ${LIBFABRIC_LIBRARIES}")
MESSAGE(STATUS "libfabric           = ${LIBFABRIC_INCLUDE_DIRS}")

include_directories(SYSTEM ${Boost_INCLUDE_DIRS} ${ZMQ_INCLUDE_DIRS} ${LIBFABRIC_INCLUDE_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBFABRIC REQUIRED_VARS LIBFABRIC_LIBRARIES LIBFABRIC_INCLUDE_DIRS)
