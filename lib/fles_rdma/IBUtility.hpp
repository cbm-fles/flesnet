// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <iostream>
#include <rdma/rdma_cma.h>

/// Overloaded output operator for ibv_wr_opcode.
inline std::ostream& operator<<(std::ostream& s, ibv_wr_opcode v) {
  switch (v) {
  case IBV_WR_RDMA_WRITE:
    return s << "IBV_WR_RDMA_WRITE";
  case IBV_WR_RDMA_WRITE_WITH_IMM:
    return s << "IBV_WR_RDMA_WRITE_WITH_IMM";
  case IBV_WR_SEND:
    return s << "IBV_WR_SEND";
  case IBV_WR_SEND_WITH_IMM:
    return s << "IBV_WR_SEND_WITH_IMM";
  case IBV_WR_RDMA_READ:
    return s << "IBV_WR_RDMA_READ";
  case IBV_WR_ATOMIC_CMP_AND_SWP:
    return s << "IBV_WR_ATOMIC_CMP_AND_SWP";
  case IBV_WR_ATOMIC_FETCH_AND_ADD:
    return s << "IBV_WR_ATOMIC_FETCH_AND_ADD";
  default:
    return s << static_cast<int>(v);
  }
}

/// Overloaded output operator for ibv_send_flags.
inline std::ostream& operator<<(std::ostream& s, ibv_send_flags v) {
  std::string str;
  if (v & IBV_SEND_FENCE)
    str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_FENCE";
  if (v & IBV_SEND_SIGNALED)
    str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_SIGNALED";
  if (v & IBV_SEND_SOLICITED)
    str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_SOLICITED";
  if (v & IBV_SEND_INLINE)
    str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_INLINE";
  if (str.empty())
    return s << static_cast<int>(v);
  else
    return s << str;
}
