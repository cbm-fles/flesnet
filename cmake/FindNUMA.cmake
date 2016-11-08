# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

find_path(NUMA_INCLUDE_DIR numa.h)
find_library(NUMA_LIBRARY numa)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NUMA REQUIRED_VARS NUMA_LIBRARY NUMA_INCLUDE_DIR)
