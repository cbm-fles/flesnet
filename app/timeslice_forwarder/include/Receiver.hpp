#pragma once
#include <TimesliceReceiver.hpp>
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


    std::shared_ptr<boost::interprocess::managed_shared_memory> get_managed_shm() {
        return managed_shm_;
    }
};
}
