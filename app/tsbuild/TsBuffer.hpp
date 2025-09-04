// Copyright 2016-2020, 2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ItemProducer.hpp"
#include "SubTimeslice.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid.hpp>
#include <log.hpp>
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
/** A TsBuffer object represents the build node's timeslice buffer
 */

class TsBuffer : public ItemProducer {
public:
  TsBuffer(zmq::context_t& context,
           const std::string& distributor_address,
           std::string shm_identifier,
           std::size_t buffer_size);

  TsBuffer(const TsBuffer&) = delete;
  void operator=(const TsBuffer&) = delete;

  ~TsBuffer();

  [[nodiscard]] std::size_t get_size() const { return m_buffer_size; }

  [[nodiscard]] std::size_t get_free_memory() const {
    return m_managed_shm->get_free_memory();
  }

  [[nodiscard]] std::byte* allocate(std::size_t size) noexcept {
    void* raw_ptr = m_managed_shm->allocate(size, std::nothrow);
    return static_cast<std::byte*>(raw_ptr);
  }

  // free bytes in the shared memory
  void deallocate(std::byte* ptr) noexcept { m_managed_shm->deallocate(ptr); }

  /// Send a work item to the item distributor.
  void send_work_item(std::byte* buffer, TsId id, const StDescriptor& ts_desc);

  /// Receive a completion from the item distributor.
  [[nodiscard]] std::optional<ItemID> try_receive_completion() {
    ItemID id;
    if (!ItemProducer::try_receive_completion(&id)) {
      return std::nullopt;
    }
    if (m_outstanding.erase(id) != 1) {
      ERROR("Invalid item with id {}", id);
    }
    return id;
  };

private:
  std::string m_shm_identifier;    ///< shared memory identifier
  boost::uuids::uuid m_shm_uuid{}; ///< shared memory UUID
  std::size_t m_buffer_size;       ///< buffer size in bytes

  std::unique_ptr<boost::interprocess::managed_shared_memory>
      m_managed_shm;              ///< shared memory object
  std::set<ItemID> m_outstanding; ///< set of outstanding work items
};
