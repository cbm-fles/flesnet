// Copyright 2013-2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceReceiver class.
#pragma once

#include "ItemWorker.hpp"
#include "ItemWorkerProtocol.hpp"
#include "Source.hpp"
#include "Timeslice.hpp"
#include "TimesliceShmWorkItem.hpp"
#include "TimesliceView.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <memory>
#include <string>
#include <utility>

namespace fles {

template <class Base, class Derived> class Receiver : public Source<Base> {
public:
  Receiver(const std::string&, WorkerParameters){};
  [[nodiscard]] bool eos() const override { return true; };

private:
  Derived* do_get() override { return nullptr; };
};

/**
 * \brief The TimesliceReceiver class implements the IPC mechanisms to receive a
 * timeslice.
 */
template <>
class Receiver<Timeslice, TimesliceView> : public Source<Timeslice> {
public:
  /// Construct timeslice receiver connected to a given shared memory.
  Receiver(const std::string& ipc_identifier, WorkerParameters parameters)
      : worker_("ipc://@" + ipc_identifier, std::move(parameters)) {
    worker_.set_disconnect_callback([this] { managed_shm_ = nullptr; });
  }

  /// Delete copy constructor (non-copyable).
  Receiver(const Receiver&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const Receiver&) = delete;

  ~Receiver() override = default;

  /**
   * \brief Retrieve the next item.
   *
   * This function blocks if the next item is not yet available.
   *
   * \return pointer to the item, or nullptr if end-of-file
   */
  std::unique_ptr<TimesliceView> get() {
    return std::unique_ptr<TimesliceView>(do_get());
  };

  [[nodiscard]] bool eos() const override { return eos_; }

private:
  TimesliceView* do_get() override {
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
                boost::interprocess::open_read_only,
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

      return new TimesliceView(managed_shm_, item, timeslice_item); // NOLINT
    }

    eos_ = true;
    return nullptr;
  }

  std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;

  [[nodiscard]] boost::uuids::uuid managed_shm_uuid() const {
    if (!managed_shm_) {
      return boost::uuids::nil_uuid();
    }
    auto* shm_uuid =
        managed_shm_
            ->find<boost::uuids::uuid>(boost::interprocess::unique_instance)
            .first;
    assert(shm_uuid != nullptr);
    return *shm_uuid;
  }

  /// The end-of-stream flag.
  bool eos_ = false;

  // The respective item worker object
  ItemWorker worker_;
};

} // namespace fles
