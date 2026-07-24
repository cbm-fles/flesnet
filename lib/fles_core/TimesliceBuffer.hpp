// Copyright 2016-2020 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ItemProducer.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceWorkItem.hpp"
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <string>

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

  /// Get the 2's exponent of the size of the data buffer per input node in
  /// bytes.
  [[nodiscard]] uint32_t get_data_size_exp() const {
    return data_buffer_size_exp_;
  }

  /// Get the 2's exponent of the size of the descriptor buffer per input node
  /// in units of TimesliceComponentDescriptors.
  [[nodiscard]] uint32_t get_desc_size_exp() const {
    return desc_buffer_size_exp_;
  }

  /// Get the pointer to the data buffer of the specified input node.
  [[nodiscard]] uint8_t* get_data_ptr(uint_fast16_t index) const {
    return data_ptr_ + index * (UINT64_C(1) << data_buffer_size_exp_);
  }

  /// Get the pointer to the descriptor buffer of the specified input node.
  [[nodiscard]] fles::TimesliceComponentDescriptor*
  get_desc_ptr(uint_fast16_t index) {
    return desc_ptr_ + index * (UINT64_C(1) << desc_buffer_size_exp_);
  }

  /// Get the reference to the data buffer of the specified input node at the
  /// given offset.
  [[nodiscard]] uint8_t& get_data(uint_fast16_t index, uint64_t offset) const {
    offset &= (UINT64_C(1) << data_buffer_size_exp_) - 1;
    return get_data_ptr(index)[offset];
  }

  /// Get the reference to the descriptor buffer of the specified input node at
  /// the given offset.
  [[nodiscard]] fles::TimesliceComponentDescriptor&
  get_desc(uint_fast16_t index, uint64_t offset) {
    offset &= (UINT64_C(1) << desc_buffer_size_exp_) - 1;
    return get_desc_ptr(index)[offset];
  }

  /// Get the number of input nodes.
  [[nodiscard]] uint32_t get_num_input_nodes() const {
    return num_input_nodes_;
  }

  /// Send a work item to the item distributor.
  void send_work_item(fles::TimesliceWorkItem wi);

  /// Receive a completion from the item distributor.
  [[nodiscard]] bool try_receive_completion(fles::TimesliceCompletion& c) {
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

  [[nodiscard]] std::size_t get_num_work_items() const {
    return outstanding_.size();
  }

  [[nodiscard]] std::size_t get_num_completions() const { return 0; }

  [[nodiscard]] std::string description() const;

private:
  std::string shm_identifier_;    ///< shared memory identifier
  boost::uuids::uuid shm_uuid_;   ///< shared memory UUID
  uint32_t data_buffer_size_exp_; ///< 2's exponent of data buffer size in bytes
  uint32_t desc_buffer_size_exp_; ///< 2's exponent of descriptor buffer size
                                  ///< in units of TimesliceComponentDescriptors
  uint32_t num_input_nodes_;      // number of input nodes

  std::unique_ptr<boost::interprocess::managed_shared_memory>
      managed_shm_;   ///< shared memory object
  uint8_t* data_ptr_; ///< pointer to data buffer within shared memory
  fles::TimesliceComponentDescriptor*
      desc_ptr_;                 ///< pointer to descriptor
                                 ///< buffer within shared memory
  std::set<ItemID> outstanding_; ///< set of outstanding work items
};
