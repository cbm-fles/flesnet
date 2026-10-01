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
#include <cstdint>
#include <map>
#include <memory>
#include <new>
#include <optional>
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
 * The caller allocates the memory for the components of a timeslice, in one
 * or several allocations. Once a timeslice is written, send_work_item()
 * announces it. When all workers are done with it, try_receive_completion()
 * hands back its id and the caller's user data, and the memory can be
 * released.
 *
 * The work items are numbered consecutively, independent of the timeslice
 * index. The workers select items by this number (stride and offset), so the
 * selection applies to the sequence of timeslices published here, whatever
 * subset of the global timeslice stream that is. Timeslice indices may also
 * repeat, e.g., when an archive is replayed.
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

  /// The offset of a location in the segment, as used in the descriptors.
  [[nodiscard]] std::ptrdiff_t offset_of(const std::byte* ptr) const {
    return ptr - static_cast<const std::byte*>(m_managed_shm->get_address());
  }

  /// A published timeslice that is no longer in use
  struct Completion {
    tsb::TsId id;            ///< timeslice index
    std::uint64_t user_data; ///< value given to send_work_item()
  };

  /// Announce a timeslice stored in the segment. The component offsets in the
  /// descriptor are relative to the start of the segment (see offset_of()), so
  /// the components can be placed independently. The user data is handed back
  /// on completion, e.g., to identify the memory to release.
  void send_work_item(tsb::TsId id,
                      const tsb::StDescriptor& ts_desc,
                      std::uint64_t user_data = 0);

  /// Announce that no more timeslices will follow.
  void send_end_of_stream() { m_producer.send_end_of_stream(); }

  /// Receive a timeslice that is no longer in use, if any.
  [[nodiscard]] std::optional<Completion> try_receive_completion();

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

  ItemID m_next_item_id = 0; ///< number of the next work item

  /// The timeslices of the outstanding work items
  std::map<ItemID, Completion> m_outstanding;
};

} // namespace fles
