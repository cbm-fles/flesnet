// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceView class.
#pragma once

#include "ItemWorkerProtocol.hpp"
#include "SubTimeslice.hpp"
#include "Timeslice.hpp"
#include "TimesliceShmWorkItem.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid.hpp>
#include <cstddef>
#include <memory>
#include <span>

namespace fles {

template <class Base, class View> class Receiver;

/**
 * \brief The TimesliceView class provides access to the data of a single
 * timeslice in memory.
 */
class TimesliceView : public Timeslice {
public:
  /// Delete copy constructor (non-copyable).
  TimesliceView(const TimesliceView&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const TimesliceView&) = delete;

  ~TimesliceView() override = default;

  /// The UUID of the shared memory segment containing the timeslice. It
  /// changes when the producer creates a new segment.
  [[nodiscard]] const boost::uuids::uuid& shm_uuid() const {
    return timeslice_item_.shm_uuid;
  }

  /// The complete mapped shared memory segment, e.g., for registering it with
  /// an RDMA device. It stays mapped as long as a view into it exists.
  [[nodiscard]] std::span<const std::byte> shm_region() const {
    return {static_cast<const std::byte*>(managed_shm_->get_address()),
            managed_shm_->get_size()};
  }

  /// The contiguous memory range within the segment that holds the data of all
  /// components.
  [[nodiscard]] std::span<const std::byte> data_block() const;

  /// The timeslice descriptor in the layout expected by
  /// TimesliceShmBuffer::send_work_item(), with component offsets relative to
  /// data_block().
  [[nodiscard]] tsb::StDescriptor st_descriptor() const;

private:
  friend class Receiver<Timeslice, TimesliceView>;
  friend class StorableTimeslice;

  TimesliceView(
      std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm,
      std::shared_ptr<const Item> work_item,
      const TimesliceShmWorkItem& timeslice_item);

  std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;
  std::shared_ptr<const Item> work_item_;
  fles::TimesliceShmWorkItem timeslice_item_;
};

} // namespace fles
