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
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace fles {

template <class Base, class Derived> class Receiver : public Source<Base> {
public:
  Receiver(const std::string&, WorkerParameters) {};
  [[nodiscard]] bool eos() const override { return true; };
  void set_state_callback(ItemWorker::StateCallback){};
  [[nodiscard]] ConnectionState connection_state() const {
    return ConnectionState::Disconnected;
  };

private:
  Derived* do_get() override { return nullptr; };
};

/// Access mode for the shared memory segment opened by a timeslice receiver.
enum class ShmAccess {
  ReadOnly, ///< Map the segment read-only (default)
  ReadWrite ///< Map the segment writable, e.g., for RDMA memory registration
};

/**
 * \brief The TimesliceReceiver class implements the IPC mechanisms to receive a
 * timeslice.
 */
template <>
class Receiver<Timeslice, TimesliceView> : public Source<Timeslice> {
public:
  /// Construct timeslice receiver connected to a given shared memory.
  Receiver(const std::string& ipc_identifier,
           WorkerParameters parameters,
           ShmAccess access = ShmAccess::ReadOnly)
      : worker_("ipc://@" + ipc_identifier, std::move(parameters)),
        access_(access) {
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
  std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;

  /**
   * \brief Install a callback invoked on every connection state change.
   *
   * Must be called before the first call to get().
   */
  void set_state_callback(ItemWorker::StateCallback callback) {
    worker_.set_state_callback(std::move(callback));
  }

  [[nodiscard]] ConnectionState connection_state() const {
    return worker_.connection_state();
  }

  /// Identifier of the distributor instance this receiver is connected to.
  [[nodiscard]] std::string distributor_instance_id() const {
    return worker_.distributor_instance_id();
  }

  [[nodiscard]] size_t items_received() const {
    return worker_.items_received();
  }
  [[nodiscard]] size_t items_completed() const {
    return worker_.items_completed();
  }
  [[nodiscard]] size_t reconnect_count() const {
    return worker_.reconnect_count();
  }

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

private:
  TimesliceView* do_get() override {
    if (eos_) {
      return nullptr;
    }

    while (auto item = worker_.get()) {
      fles::TimesliceShmWorkItem timeslice_item;
      std::istringstream istream(item->payload());
      try {
        boost::archive::binary_iarchive iarchive(istream);
        iarchive >> timeslice_item;
      } catch (const boost::archive::archive_exception& e) {
        throw std::runtime_error(
            std::string("TimesliceReceiver: cannot read work item, producer "
                        "uses an incompatible format: ") +
            e.what());
      }

      // connect to matching shared memory if not already connected
      if (managed_shm_uuid() != timeslice_item.shm_uuid) {
        if (access_ == ShmAccess::ReadWrite) {
          managed_shm_ =
              std::make_unique<boost::interprocess::managed_shared_memory>(
                  boost::interprocess::open_only,
                  timeslice_item.shm_identifier.c_str());
        } else {
          managed_shm_ =
              std::make_unique<boost::interprocess::managed_shared_memory>(
                  boost::interprocess::open_read_only,
                  timeslice_item.shm_identifier.c_str());
        }
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

      if (timeslice_item.tsc_desc.size() !=
              timeslice_item.ts_desc.num_components ||
          timeslice_item.data.size() != timeslice_item.ts_desc.num_components) {
        throw std::runtime_error("TimesliceReceiver: invalid work item, "
                                 "component count mismatch");
      }

      return new TimesliceView(managed_shm_, item, timeslice_item); // NOLINT
    }

    eos_ = true;
    return nullptr;
  }

  // std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;



  /// The end-of-stream flag.
  bool eos_ = false;

  // The respective item worker object
  ItemWorker worker_;

  /// The access mode for the shared memory segment
  ShmAccess access_;
};

} // namespace fles
