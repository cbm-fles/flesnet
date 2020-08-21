// Copyright 2019 Farouk Salem <salem@zib.de>

/**
 * An implementation of fi_context object pool based on the Object Pool Design
 * Pattern
 */
#pragma once

#include <list>
#include <log.hpp>
#include <memory>
#include <mutex>
#include <rdma/fabric.h>
#include <string.h>

namespace tl_libfabric {
struct fi_custom_context {
  struct fi_context context;
  uint64_t id;
  uint64_t op_context;
};

class LibfabricContextPool {
public:
  ~LibfabricContextPool();

  LibfabricContextPool(const LibfabricContextPool&) = delete;
  LibfabricContextPool& operator=(const LibfabricContextPool&) = delete;

  struct fi_custom_context* getContext();

  void releaseContext(struct fi_custom_context* context);

  static std::unique_ptr<LibfabricContextPool>& getInst();

private:
  static std::unique_ptr<LibfabricContextPool> context_pool_;
  std::list<struct fi_custom_context*> available_;
  std::list<struct fi_custom_context*> in_use_;

  uint64_t context_counter_ = 0;

  std::mutex pool_mutex_;

  LibfabricContextPool();

  void log();
};
} // namespace tl_libfabric
