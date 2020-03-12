// Copyright 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceCompletion.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceWorkItem.hpp"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <csignal>

/// Timeslice buffer container class.
/** A TimesliceBuffer object represents the compute node's timeslice buffer
   (filled by the input nodes). */

class TimesliceBuffer {
public:
  /// The TimesliceBuffer constructor.
  TimesliceBuffer(std::string shm_identifier,
                  uint32_t data_buffer_size_exp,
                  uint32_t desc_buffer_size_exp,
                  uint32_t num_input_nodes);

  TimesliceBuffer(const TimesliceBuffer&) = delete;
  void operator=(const TimesliceBuffer&) = delete;

  /// The TimesliceBuffer destructor.
  ~TimesliceBuffer();

  uint32_t get_data_size_exp() const { return data_buffer_size_exp_; }
  uint32_t get_desc_size_exp() const { return desc_buffer_size_exp_; }

  uint8_t* get_data_ptr(uint_fast16_t index);
  fles::TimesliceComponentDescriptor* get_desc_ptr(uint_fast16_t index);

  uint8_t& get_data(uint_fast16_t index, uint64_t offset);
  fles::TimesliceComponentDescriptor& get_desc(uint_fast16_t index,
                                               uint64_t offset);

  uint32_t get_num_input_nodes() const { return num_input_nodes_; }

  void send_work_item(fles::TimesliceWorkItem wi) {
    work_items_mq_->send(&wi, sizeof(wi), 0);
  }

  void send_completion(fles::TimesliceCompletion c) {
    completions_mq_->send(&c, sizeof(c), 0);
  }

  void send_end_work_item() { work_items_mq_->send(nullptr, 0, 0); }

  void send_end_completion() { completions_mq_->send(nullptr, 0, 0); }

  std::size_t get_num_work_items() const {
    return work_items_mq_->get_num_msg();
  }

  std::size_t get_num_completions() const {
    return completions_mq_->get_num_msg();
  }

  bool try_receive_completion(fles::TimesliceCompletion& c) {
    std::size_t recvd_size;
    unsigned int priority;
    if (!completions_mq_->try_receive(&c, sizeof(c), recvd_size, priority)) {
      return false;
    }
    if (recvd_size == 0) {
      return false;
    }
    assert(recvd_size == sizeof(c));
    return true;
  };

private:
  std::string shm_identifier_;

  uint32_t data_buffer_size_exp_;
  uint32_t desc_buffer_size_exp_;

  uint32_t num_input_nodes_;

  std::unique_ptr<boost::interprocess::shared_memory_object> data_shm_;
  std::unique_ptr<boost::interprocess::shared_memory_object> desc_shm_;

  std::unique_ptr<boost::interprocess::mapped_region> data_region_;
  std::unique_ptr<boost::interprocess::mapped_region> desc_region_;

  std::unique_ptr<boost::interprocess::message_queue> work_items_mq_;
  std::unique_ptr<boost::interprocess::message_queue> completions_mq_;
};
