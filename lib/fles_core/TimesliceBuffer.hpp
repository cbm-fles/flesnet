// Copyright 2016-2020 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ItemProducer.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace fles {
struct TimesliceWorkItem;
}
namespace zmq {
class context_t;
}

/// Timeslice buffer container class.
/** A TimesliceBuffer object represents the compute node's timeslice buffer
   (filled by the input nodes). */

class TimesliceBuffer : public ItemProducer {
public:
  /// The TimesliceBuffer constructor.
  TimesliceBuffer(zmq::context_t& context,
                  const std::string& distributor_address,
                  std::string shm_identifier,
                  uint32_t data_buffer_size_exp,
                  uint32_t desc_buffer_size_exp,
                  uint32_t num_input_nodes);

  TimesliceBuffer(const TimesliceBuffer&) = delete;
  void operator=(const TimesliceBuffer&) = delete;

  /// The TimesliceBuffer destructor.
  ~TimesliceBuffer();

  [[nodiscard]] uint32_t get_data_size_exp() const {
    return data_buffer_size_exp_;
  }
  [[nodiscard]] uint32_t get_desc_size_exp() const {
    return desc_buffer_size_exp_;
  }

  uint8_t* get_data_ptr(uint_fast16_t index) {
    return data_ptr_ + index * (UINT64_C(1) << data_buffer_size_exp_);
  }

  fles::TimesliceComponentDescriptor* get_desc_ptr(uint_fast16_t index) {
    return desc_ptr_ + index * (UINT64_C(1) << desc_buffer_size_exp_);
  }

  uint8_t& get_data(uint_fast16_t index, uint64_t offset) {
    offset &= (UINT64_C(1) << data_buffer_size_exp_) - 1;
    return get_data_ptr(index)[offset];
  }

  fles::TimesliceComponentDescriptor& get_desc(uint_fast16_t index,
                                               uint64_t offset) {
    offset &= (UINT64_C(1) << desc_buffer_size_exp_) - 1;
    return get_desc_ptr(index)[offset];
  }

  [[nodiscard]] uint32_t get_num_input_nodes() const {
    return num_input_nodes_;
  }

  void send_work_item(fles::TimesliceWorkItem wi);

  bool try_receive_completion(fles::TimesliceCompletion& c) {
    ItemID id;
    if (!ItemProducer::try_receive_completion(&id)) {
      return false;
    }
    if (outstanding_.erase(id) != 1) {
      std::cerr << "Error: invalid item " << id << std::endl;
    }
    c.ts_pos = id;
    return true;
  };

  // Remaining member functions are for backwards compatibility only

  void send_completion(fles::TimesliceCompletion /* c */) {
    throw std::runtime_error(
        "TimesliceBuffer::send_completion() not supported");
  }

  void send_end_work_item() {}

  void send_end_completion() {}

  [[nodiscard]] std::size_t get_num_work_items() const {
    return outstanding_.size();
  }

  [[nodiscard]] std::size_t get_num_completions() const { return 0; }

  [[nodiscard]] std::string description() const;

private:
  std::string shm_identifier_;
  boost::uuids::uuid shm_uuid_{};
  uint32_t data_buffer_size_exp_;
  uint32_t desc_buffer_size_exp_;
  uint32_t num_input_nodes_;

  std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shm_;
  uint8_t* data_ptr_;
  fles::TimesliceComponentDescriptor* desc_ptr_;
  std::set<ItemID> outstanding_;
};
