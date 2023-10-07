// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceReceiver.hpp"
#include "TimesliceShmWorkItem.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <memory>

namespace bi = boost::interprocess;

namespace fles {

TimesliceReceiver::TimesliceReceiver(const std::string& ipc_identifier,
                                     WorkerParameters parameters)
    : worker_("ipc://@" + ipc_identifier, parameters) {
  worker_.set_disconnect_callback([this] { managed_shm_ = nullptr; });
}

TimesliceView* TimesliceReceiver::do_get() {
  if (eos_) {
    return nullptr;
  }

  while (auto item = worker_.get()) {
    fles::TimesliceShmWorkItem timeslice_item;
    std::istringstream istream(item->payload());
    {
      boost::archive::binary_iarchive iarchive(istream);
      iarchive >> timeslice_item;
    }

    // connect to matching shared memory if not already connected
    if (managed_shm_uuid() != timeslice_item.shm_uuid) {
      managed_shm_ =
          std::make_unique<boost::interprocess::managed_shared_memory>(
              boost::interprocess::open_only,
              timeslice_item.shm_identifier.c_str());
      std::cout << "TimesliceReceiver: opened shared memory "
                << timeslice_item.shm_identifier << " {" << managed_shm_uuid()
                << "}" << std::endl;
      if (managed_shm_uuid() != timeslice_item.shm_uuid) {
        std::cerr
            << "TimesliceView: discarding item due to shm uuid mismatch (shm: "
            << managed_shm_uuid() << ", ts_item: " << timeslice_item.shm_uuid
            << ")" << std::endl;
        continue;
      }
    }

    return new TimesliceView(managed_shm_, item, timeslice_item);
  }

  eos_ = true;
  return nullptr;
}

boost::uuids::uuid TimesliceReceiver::managed_shm_uuid() const {
  if (!managed_shm_) {
    return boost::uuids::nil_uuid();
  }
  auto shm_uuid =
      managed_shm_->find<boost::uuids::uuid>(bi::unique_instance).first;
  assert(shm_uuid != nullptr);
  return *shm_uuid;
}

} // namespace fles
