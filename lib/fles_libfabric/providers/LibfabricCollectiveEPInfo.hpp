// Copyright 2020 Farouk Salem <salem@zib.de>

/**
 * An implementation of fi_barrier
 */
#pragma once

#include <log.hpp>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>

namespace tl_libfabric {

struct LibfabricCollectiveEPInfo {
  uint64_t index;
  struct fid_ep* ep = nullptr;
  fi_addr_t fi_addr = FI_ADDR_UNSPEC;

  struct fi_msg_tagged recv_msg_wr;
  struct iovec recv_sge = iovec();
  struct fid_mr* mr_recv = nullptr;
  unsigned char recv_buffer[64];

  struct fi_msg_tagged send_msg_wr;
  struct iovec send_sge = iovec();
  struct fid_mr* mr_send = nullptr;
  unsigned char send_buffer[64];

  bool root_ep = false;
  bool active = true;
};
} // namespace tl_libfabric
