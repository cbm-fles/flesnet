message(STATUS "Looking for non-system ZeroMQ...")

set(ZMQ_H zmq.h)
set(ZMQ_UTILS_H zmq_utils.h)
set(ZMQ_HPP zmq.hpp)
if(APPLE)
  set(LIBZMQ_SHARED libzmq.dylib)
else()
  set(LIBZMQ_SHARED libzmq.so)
endif()
set(LIBZMQ_STATIC libzmq.a)

# Find ZMQ libraries in external packages.
# Do not search somewhere else.

find_path(ZMQ_INCLUDE_1 NAMES ${ZMQ_H} ${ZMQ_UTILS_H}
  PATHS ${SIMPATH}/include
  NO_DEFAULT_PATH
  DOC   "Path to ZeroMQ include header files."
)

find_path(ZMQ_INCLUDE_2 NAMES ${ZMQ_HPP}
  PATHS ${SIMPATH}/include $ENV{VMCWORKDIR}/fles/cppzmq
  NO_DEFAULT_PATH
  DOC   "Path to ZeroMQ CPP header files."
)

set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_1} ${ZMQ_INCLUDE_2})

find_library(ZMQ_LIBRARY_SHARED NAMES ${LIBZMQ_SHARED}
  PATHS ${SIMPATH}/lib
  NO_DEFAULT_PATH
  DOC   "Path to ${LIBZMQ_SHARED}."
)

find_library(ZMQ_LIBRARY_STATIC NAMES ${LIBZMQ_STATIC}
  PATHS ${SIMPATH}/lib
  NO_DEFAULT_PATH 
  DOC   "Path to ${LIBZMQ_STATIC}."
)

IF(ZMQ_INCLUDE_1 AND ZMQ_INCLUDE_2 AND ZMQ_LIBRARY_SHARED AND ZMQ_LIBRARY_STATIC)
  SET(ZMQ_FOUND TRUE)
ELSE(ZMQ_INCLUDE_1 AND ZMQ_INCLUDE_2 AND ZMQ_LIBRARY_SHARED AND ZMQ_LIBRARY_STATIC)
  SET(ZMQ_FOUND FALSE)
ENDIF(ZMQ_INCLUDE_1 AND ZMQ_INCLUDE_2 AND ZMQ_LIBRARY_SHARED AND ZMQ_LIBRARY_STATIC)

if(ZMQ_FOUND)
  set(ZMQ_LIBRARIES "${ZMQ_LIBRARY_STATIC};${ZMQ_LIBRARY_SHARED}")
#  set(ZMQ_LIBRARIES "${ZMQ_LIBRARY_SHARED}")
  if(NOT ExternalZMQ_FIND_QUIETLY)
    message(STATUS "Looking for non-system ZeroMQ... - found ${ZMQ_LIBRARIES}")
    message(STATUS "Looking for non-system ZeroMQ... - include ${ZMQ_INCLUDE_DIRS}")
  endif(NOT ExternalZMQ_FIND_QUIETLY)
else(ZMQ_FOUND)
    if(ExternalZMQ_FIND_REQUIRED)
      message(FATAL_ERROR "Looking for non-system ZeroMQ... - Not found")
    else(ExternalZMQ_FIND_REQUIRED)
      if(NOT ExternalZMQ_FIND_QUIETLY)
        message(STATUS "Looking for non-system ZeroMQ... - Not found")
      endif(NOT ExternalZMQ_FIND_QUIETLY)
    endif(ExternalZMQ_FIND_REQUIRED)
endif(ZMQ_FOUND)

mark_as_advanced(ZMQ_INCLUDE_DIRS ZMQ_LIBRARIES ZMQ_LIBRARY_SHARED ZMQ_LIBRARY_STATIC)
