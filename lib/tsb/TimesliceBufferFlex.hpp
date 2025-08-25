// Copyright 2016-2020, 2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ItemProducer.hpp"
#include "SubTimeslice.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>

namespace fles {
struct TimesliceWorkItem;
}
namespace zmq {
class context_t;
}

/// Timeslice buffer container class.
/** A TimesliceBufferFlex object represents the build node's timeslice buffer
 */

class TimesliceBufferFlex : public ItemProducer {
public:
  TimesliceBufferFlex(zmq::context_t& context,
                      const std::string& distributor_address,
                      std::string shm_identifier,
                      std::size_t buffer_size);

  TimesliceBufferFlex(const TimesliceBufferFlex&) = delete;
  void operator=(const TimesliceBufferFlex&) = delete;

  ~TimesliceBufferFlex();

  [[nodiscard]] std::size_t get_size() const { return buffer_size_; }

  [[nodiscard]] std::size_t get_free_memory() const {
    return managed_shm_->get_free_memory();
  }

  [[nodiscard]] std::byte* allocate(std::size_t size) noexcept {
    void* raw_ptr = managed_shm_->allocate(size, std::nothrow);
    return static_cast<std::byte*>(raw_ptr);
  }

  // free bytes in the shared memory
  void deallocate(std::byte* ptr) noexcept { managed_shm_->deallocate(ptr); }

  /// Send a work item to the item distributor.
  void send_work_item(std::byte* buffer, TsID id, const StDescriptor& ts_desc);

  /// Receive a completion from the item distributor.
  [[nodiscard]] std::optional<ItemID> try_receive_completion() {
    ItemID id;
    if (!ItemProducer::try_receive_completion(&id)) {
      return std::nullopt;
    }
    if (outstanding_.erase(id) != 1) {
      std::cerr << "Error: invalid item " << id << std::endl;
    }
    return id;
  };

private:
  std::string shm_identifier_;    ///< shared memory identifier
  boost::uuids::uuid shm_uuid_{}; ///< shared memory UUID
  std::size_t buffer_size_;       ///< buffer size in bytes

  std::unique_ptr<boost::interprocess::managed_shared_memory>
      managed_shm_;              ///< shared memory object
  std::set<ItemID> outstanding_; ///< set of outstanding work items
};
