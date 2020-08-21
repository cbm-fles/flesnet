// Copyright 2019 Farouk Salem <salem@zib.de>

#include "LibfabricContextPool.hpp"

namespace tl_libfabric {

std::unique_ptr<LibfabricContextPool>& LibfabricContextPool::getInst() {
  if (LibfabricContextPool::context_pool_ == nullptr)
    LibfabricContextPool::context_pool_ =
        std::unique_ptr<LibfabricContextPool>(new LibfabricContextPool());

  return LibfabricContextPool::context_pool_;
}

LibfabricContextPool::LibfabricContextPool() {
  ///
}
LibfabricContextPool::~LibfabricContextPool() {

  for (auto it = available_.begin(); it != available_.end(); it++) {
    delete (*it);
  }

  for (auto it = in_use_.begin(); it != in_use_.end(); it++) {
    delete (*it);
  }

  available_.clear();
  in_use_.clear();
  L_(info) << "LibfabricContextPool deconstructor: Total number of created "
              "objects "
           << context_counter_;
}

struct fi_custom_context* LibfabricContextPool::getContext() {
  pool_mutex_.lock();
  if (available_.empty()) {
    struct fi_custom_context* context = new fi_custom_context();
    context->id = context_counter_++;
    in_use_.push_front(context);
    L_(debug) << "getContext:: New context is created with ID " << context->id;
  } else {
    in_use_.push_front(available_.front());
    available_.pop_front();
  }
  pool_mutex_.unlock();
  return (*in_use_.begin());
}

void LibfabricContextPool::releaseContext(struct fi_custom_context* context) {
  uint32_t count = 0;
  for (auto it = in_use_.begin(); it != in_use_.end(); it++) {
    if (context->id == (*it)->id) {
      available_.push_back((*it));
      in_use_.erase(it);
      break;
    }
    ++count;
  }
  L_(debug) << "LibfabricContextPool: Context released with ID " << context->id;
}

void LibfabricContextPool::log() {
  int i = 0;
  for (auto it = available_.begin(); it != available_.end(); it++, i++)
    L_(debug) << "Logging:: Available[" << i << "].id = " << (*it)->id;

  i = 0;
  for (auto it = in_use_.begin(); it != in_use_.end(); it++, i++)
    L_(debug) << "Logging:: in_use_[" << i << "].id = " << (*it)->id;
}

std::unique_ptr<LibfabricContextPool> LibfabricContextPool::context_pool_ =
    nullptr;

} // namespace tl_libfabric
