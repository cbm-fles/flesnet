# Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

find_path(MPI_INCLUDE_DIR NAMES mpi.h PATH_SUFFIXES mpi mpi/include)
find_library(MPI_LIBRARY NAMES mpi)

MESSAGE(STATUS "MPI           = ${MPI_LIBRARY}")
MESSAGE(STATUS "MPI           = ${MPI_INCLUDE_DIR}")

#SET(CMAKE_C_COMPILER /usr/local/bin/mpicc)
#SET(CMAKE_CXX_COMPILER /usr/local/bin/mpicxx)

#include_directories(SYSTEM ${MPI_INCLUDE_DIR})
#add_definitions(-DOMPI_SKIP_MPICXX)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPI REQUIRED_VARS MPI_LIBRARY MPI_INCLUDE_DIR)
