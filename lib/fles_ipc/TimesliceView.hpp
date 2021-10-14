// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceView class.
#pragma once

#include "ItemWorkerProtocol.hpp"
#include "Timeslice.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceShmWorkItem.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <memory>

namespace fles {

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

private:
  friend class TimesliceReceiver;
  friend class StorableTimeslice;

  TimesliceView(
      std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm,
      std::shared_ptr<const Item> work_item,
      TimesliceShmWorkItem timeslice_item);

  std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;
  std::shared_ptr<const Item> work_item_;
  fles::TimesliceShmWorkItem timeslice_item_;
};

} // namespace fles
