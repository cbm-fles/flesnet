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
private:
  TimesliceView* do_get() override {
    std::cout << "do get" << std::endl;
    if (eos_) {
      return nullptr;
    }

          std::cout << "do get 1" << std::endl;

    while (auto item = worker_.get()) {
      std::cout << "do get 2" << std::endl;

      fles::TimesliceShmWorkItem timeslice_item;
      std::istringstream istream(item->payload());
      std::cout << "do get 3" << std::endl;

      {
        boost::archive::binary_iarchive iarchive(istream);
        iarchive >> timeslice_item;
      }
      // std::cout << "timeslice_item.data.size(): " << timeslice_item.data.size() << std::endl;
      // std::cout << "timeslice_item.desc.size(): " << timeslice_item.desc.size() << std::endl;
      // // std::cout << "timeslice_item.desc.size(): " << timeslice_item. << std::endl;
      // for (auto const& d : timeslice_item.desc) {
      //   std::cout << "desc_ptr: " << d << std::endl;
      // }
      // for (auto const& d : timeslice_item.data) {
      //   std::cout << "data_ptr: " << d << std::endl;
      // }
      // std::cout << "timeslice_item.desc.size(): " << timeslice_item.desc.size() << std::endl;
      // timeslice_item.desc.size();
      std::cout << "do get 4" << std::endl;

      // connect to matching shared memory if not already connected
      if (managed_shm_uuid() != timeslice_item.shm_uuid) {
        // managed_shm_ =
        //     std::make_unique<boost::interprocess::managed_shared_memory>(
        //         boost::interprocess::open_read_only,
        //         timeslice_item.shm_identifier.c_str());
      std::cout << "do get 4.1: timeslice_item.shm_identifier: " << timeslice_item.shm_identifier << " (" << timeslice_item.shm_identifier.size() << ")" << std::endl;
      std::cout << ".1: timeslice_item.shm_uuid: " << timeslice_item.shm_uuid << std::endl;
        // sleep(2);
        // std::cout << "after sleep" << std::endl;
        // this memory will be used for RDMA transmissions. Therefore it needs to handle read and write calls
        try {
        managed_shm_ =
            std::make_unique<boost::interprocess::managed_shared_memory>(
                boost::interprocess::open_only,
                timeslice_item.shm_identifier.c_str());
          std::cout << "do get 4.11" << std::endl;
      } catch (boost::interprocess::interprocess_exception const& ex) {
            std::cerr << "exception caught" << std::endl;

        std::cerr << ex.what() << std::endl;
        exit(-1);
      }
        std::cout << "TimesliceReceiver: opened shared memory "
                  << timeslice_item.shm_identifier << " {" << managed_shm_uuid()
                  << "}" << std::endl;
        if (managed_shm_uuid() != timeslice_item.shm_uuid) {
    std::cout << "do get 4.2" << std::endl;

          std::cerr << "TimesliceReceiver: discarding item due to shm uuid "
                       "mismatch (shm: "
                    << managed_shm_uuid()
                    << ", ts_item: " << timeslice_item.shm_uuid << ")"
                    << std::endl;
    std::cout << "do get 4.3" << std::endl;

          continue;
        }
      }
    std::cout << "do get 5" << std::endl;
      auto *ts_view = new TimesliceView(managed_shm_, item, timeslice_item);
      std::cout << "ts_index in TimesliceBufferMap: " << ts_view->timeslice_descriptor_.index << std::endl;
      std::cout << "num_core_microslices in TimesliceBufferMap: " << ts_view->timeslice_descriptor_.num_core_microslices << std::endl;
      return ts_view; // NOLINT
    }

    eos_ = true;
    return nullptr;
  }

  // std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;



  /// The end-of-stream flag.
  bool eos_ = false;

  // The respective item worker object
  ItemWorker worker_;
};

} // namespace fles
