// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceReceiver.hpp"
#include "TimesliceShmWorkItem.hpp"
#include <memory>

namespace bi = boost::interprocess;

namespace fles {

TimesliceReceiver::TimesliceReceiver(const std::string& shm_identifier,
                                     WorkerParameters parameters)
    : shm_identifier_(shm_identifier),
      worker_("ipc://@" + shm_identifier, parameters) {
  managed_shm_ = std::make_unique<bi::managed_shared_memory>(
      bi::open_only, shm_identifier_.c_str());
}

TimesliceView* TimesliceReceiver::do_get() {
  if (eos_) {
    return nullptr;
  }

  auto item = worker_.get();
  return new TimesliceView(managed_shm_, item);
}

} // namespace fles
