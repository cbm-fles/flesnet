// Copyright 2020 Farouk Salem <salem@zib.de>

/**
 * An implementation of fi_barrier
 */
#pragma once

#include "LibfabricCollective.hpp"
#include <log.hpp>

namespace tl_libfabric {

class LibfabricBarrier : public LibfabricCollective {
public:
  LibfabricBarrier(const LibfabricBarrier&) = delete;
  LibfabricBarrier& operator=(const LibfabricBarrier&) = delete;

  size_t call_barrier();

  static void create_barrier_instance(uint32_t remote_index,
                                      struct fid_domain* pd,
                                      bool is_root);

  static LibfabricBarrier* get_instance();

private:
  ~LibfabricBarrier();
  LibfabricBarrier(uint32_t remote_index, struct fid_domain* pd, bool is_root);
  void receive_all(bool only_root_eps = false);
  void broadcast(bool only_root_eps = false);
  void recv(const struct LibfabricCollectiveEPInfo* ep_info);
  void send(const struct LibfabricCollectiveEPInfo* ep_info);

  static LibfabricBarrier* barrier_;
  bool root_;
};
} // namespace tl_libfabric
