// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceReceiver class.
#pragma once

#include "TimesliceSource.hpp"
#include "TimesliceView.hpp"
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
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
  explicit TimesliceReceiver(const std::string& shared_memory_identifier);

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

  bool eos() const override { return eos_; }

private:
  TimesliceView* do_get() override;

  const std::string shared_memory_identifier_;

  std::unique_ptr<boost::interprocess::shared_memory_object> data_shm_;
  std::unique_ptr<boost::interprocess::shared_memory_object> desc_shm_;

  std::unique_ptr<boost::interprocess::mapped_region> data_region_;
  std::unique_ptr<boost::interprocess::mapped_region> desc_region_;

  std::unique_ptr<boost::interprocess::message_queue> work_items_mq_;
  std::shared_ptr<boost::interprocess::message_queue> completions_mq_;

  /// The end-of-stream flag.
  bool eos_ = false;
};

} // namespace fles
