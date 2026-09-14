/* Copyright (C) 2016-2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
/// \file
/// \brief Defines the fles::TimesliceShmBuffer class.
#pragma once

#include "ItemDistributor.hpp"
#include "ItemProducer.hpp"
#include "SubTimeslice.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid.hpp>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <thread>

namespace zmq {
class context_t;
}

namespace fles {

/**
 * \brief The TimesliceShmBuffer class provides a shared memory segment for
 * timeslices and publishes them to the connected workers.
 *
 * Each timeslice occupies one allocation in the segment, holding the data of
 * all its components. Once a timeslice is written, send_work_item() announces
 * it. When all workers are done with it, try_receive_completion() returns its
 * id, and the allocation can be released.
 */
class TimesliceShmBuffer {
public:
  /// Create the shared memory segment and start the item distributor.
  TimesliceShmBuffer(zmq::context_t& context,
                     std::string shm_identifier,
                     std::size_t buffer_size);

  TimesliceShmBuffer(const TimesliceShmBuffer&) = delete;
  void operator=(const TimesliceShmBuffer&) = delete;

  /// Stop the item distributor and remove the shared memory segment.
  ~TimesliceShmBuffer();

  [[nodiscard]] std::size_t get_size() const { return m_buffer_size; }

  [[nodiscard]] std::span<std::byte> get_memory_region() const {
    return {static_cast<std::byte*>(m_managed_shm->get_address()),
            m_managed_shm->get_size()};
  }

  [[nodiscard]] std::size_t get_free_memory() const {
    return m_managed_shm->get_free_memory();
  }

  /// Allocate a contiguous block, return nullptr if not possible.
  [[nodiscard]] std::byte* allocate(std::size_t size) {
    return static_cast<std::byte*>(m_managed_shm->allocate(size, std::nothrow));
  }

  void deallocate(std::byte* ptr) { m_managed_shm->deallocate(ptr); }

  /// Announce a timeslice stored at the given buffer location. The component
  /// offsets in the descriptor are relative to this location.
  void send_work_item(std::byte* buffer,
                      tsb::TsId id,
                      const tsb::StDescriptor& ts_desc);

  /// Receive the id of a timeslice that is no longer in use, if any.
  [[nodiscard]] std::optional<ItemID> try_receive_completion();

  /// Check whether a timeslice with the given id is still in use.
  [[nodiscard]] bool is_outstanding(ItemID id) const {
    return m_outstanding.contains(id);
  }

  /// Check whether no timeslice is in use.
  [[nodiscard]] bool empty() const { return m_outstanding.empty(); }

private:
  std::string m_shm_identifier;    ///< shared memory identifier
  boost::uuids::uuid m_shm_uuid{}; ///< shared memory UUID
  std::size_t m_buffer_size;       ///< buffer size in bytes

  /// The item distributor, binds the addresses the producer and the workers
  /// connect to
  ItemDistributor m_item_distributor;

  std::unique_ptr<boost::interprocess::managed_shared_memory>
      m_managed_shm; ///< shared memory object

  /// The connection to the item distributor, created after the distributor
  ItemProducer m_producer;

  std::thread m_distributor_thread; ///< runs the item distributor
  std::set<ItemID> m_outstanding;   ///< set of outstanding work items
};

} // namespace fles
