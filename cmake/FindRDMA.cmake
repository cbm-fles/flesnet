# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

find_path(IBVERBS_INCLUDE_DIR infiniband/verbs.h)
find_library(IBVERBS_LIBRARY NAMES ibverbs)

find_path(RDMACM_INCLUDE_DIR rdma/rdma_cma.h)
find_library(RDMACM_LIBRARY NAMES rdmacm)

set(RDMA_INCLUDE_DIR ${IBVERBS_INCLUDE_DIR} ${RDMACM_INCLUDE_DIR})
set(RDMA_LIBRARIES ${IBVERBS_LIBRARY} ${RDMACM_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RDMA REQUIRED_VARS IBVERBS_INCLUDE_DIR IBVERBS_LIBRARY RDMACM_INCLUDE_DIR RDMACM_LIBRARY)
