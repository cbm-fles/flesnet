#pragma once
#include "Timeslice.hpp"
#include <TimesliceReceiver.hpp>
#include <TsfTimesliceView.hpp>
#include <TimesliceView.hpp>

namespace tsforwarder {

class Receiver : public fles::Receiver<fles::Timeslice, fles::TimesliceView> {
    public:
        Receiver(const std::string& ipc_identifier, WorkerParameters parameters)
      : fles::Receiver<fles::Timeslice, fles::TimesliceView>(ipc_identifier, parameters)
        {};
        /// Delete copy constructor (non-copyable).
        Receiver(const Receiver&) = delete;
        /// Delete assignment operator (non-copyable).
        void operator=(const Receiver&) = delete;

  tsforwarder::TimesliceView* do_get() override {
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
          std::cerr << "TimesliceReceiver: discarding item due to shm uuid "
                       "mismatch (shm: "
                    << managed_shm_uuid()
                    << ", ts_item: " << timeslice_item.shm_uuid << ")"
                    << std::endl;
          continue;
        }
      }

      return new tsforwarder::TimesliceView(managed_shm_, item, timeslice_item); // NOLINT
    }

    eos_ = true;
    return nullptr;
  }

  std::shared_ptr<boost::interprocess::managed_shared_memory> get_managed_shm() {
      return fles::Receiver<fles::Timeslice, fles::TimesliceView>::managed_shm_;
  }
};
}
