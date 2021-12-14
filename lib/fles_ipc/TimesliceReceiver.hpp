// Copyright 2013-2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceReceiver class.
#pragma once

#include "ItemWorker.hpp"
#include "ItemWorkerProtocol.hpp"
#include "System.hpp"
#include "TimesliceSource.hpp"
#include "TimesliceView.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid.hpp>
#include <memory>
#include <string>

namespace fles {

/**
 * \brief The TimesliceReceiver class implements the IPC mechanisms to receive a
 * timeslice.
 */
class TimesliceReceiver : public TimesliceSource {
public:
  /// Construct timeslice receiver connected to a given shared memory.
  explicit TimesliceReceiver(const std::string& ipc_identifier,
                             WorkerParameters parameters);

  /// Delete copy constructor (non-copyable).
  TimesliceReceiver(const TimesliceReceiver&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const TimesliceReceiver&) = delete;

  ~TimesliceReceiver() override = default;

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
  TimesliceView* do_get() override;

  std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;

  [[nodiscard]] boost::uuids::uuid managed_shm_uuid() const;

  /// The end-of-stream flag.
  bool eos_ = false;

  // The respective item worker object
  ItemWorker worker_;
};

} // namespace fles
