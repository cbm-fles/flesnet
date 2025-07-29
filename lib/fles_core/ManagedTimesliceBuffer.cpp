// Copyright 2023 Jan de Cuveland <cmail@cuveland.de>

#include "ManagedTimesliceBuffer.hpp"
#include "Timeslice.hpp"
#include "TimesliceCompletion.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional> // ref
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <zmq.hpp>


ManagedTimesliceBuffer::ManagedTimesliceBuffer(
    zmq::context_t& context,
    const std::string& shm_identifier,
    uint32_t data_buffer_size_exp,
    uint32_t desc_buffer_size_exp,
    uint32_t num_input_nodes)
    : producer_address_("inproc://" + shm_identifier),
      worker_address_("ipc://@" + shm_identifier),
      item_distributor_(context, producer_address_, worker_address_),
      timeslice_buffer_(context,
                        producer_address_,
                        shm_identifier,
                        data_buffer_size_exp,
                        desc_buffer_size_exp,
                        num_input_nodes),
      ack_(desc_buffer_size_exp),
      distributor_thread_(std::ref(item_distributor_)) {
  for (uint32_t i = 0; i < num_input_nodes; ++i) {
    desc_.emplace_back(timeslice_buffer_.get_desc_ptr(i),
                       timeslice_buffer_.get_desc_size_exp());
    data_.emplace_back(timeslice_buffer_.get_data_ptr(i),
                       timeslice_buffer_.get_data_size_exp());
  }
}

ManagedTimesliceBuffer::~ManagedTimesliceBuffer() {
  item_distributor_.stop();
  distributor_thread_.join();
}

void ManagedTimesliceBuffer::handle_timeslice_completions() {
  fles::TimesliceCompletion c{};
  while (timeslice_buffer_.try_receive_completion(c)) {
    if (c.ts_pos == acked_) {
      do {
        ++acked_;
      } while (ack_.at(acked_) > c.ts_pos);
      for (std::size_t i = 0; i < desc_.size(); ++i) {
        desc_.at(i).set_read_index(acked_);
        data_.at(i).set_read_index(desc_.at(i).at(acked_ - 1).offset +
                                   desc_.at(i).at(acked_ - 1).size);
      }
    } else {
      ack_.at(c.ts_pos) = c.ts_pos;
    }
  }
}

bool ManagedTimesliceBuffer::timeslice_fits_in_buffer(
    const fles::Timeslice& timeslice) {
  for (uint64_t i = 0; i < timeslice.num_components(); ++i) {
    if (data_.at(i).size_available_contiguous() < timeslice.size_component(i) ||
        desc_.at(i).size_available() < 1) {
      return false;
    }
  }
  return true;
}

void ManagedTimesliceBuffer::put(
    std::shared_ptr<const fles::Timeslice> timeslice) {

  // The existing shared memory TimesliceBuffer has to support the correct
  // number of input nodes.
  if (timeslice->num_components() != timeslice_buffer_.get_num_input_nodes()) {
    throw std::runtime_error("Timeslice has wrong number of components");
  }
  // Poll for timeslice completions until enough space is available.
  handle_timeslice_completions();
  while (!timeslice_fits_in_buffer(*timeslice)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    handle_timeslice_completions();
  }

  // Copy each component to the shared memory buffer.
  for (uint64_t i = 0; i < timeslice->num_components(); ++i) {

    // Skip remaining bytes in the data buffer to avoid a fragmented entry.
    data_.at(i).skip_buffer_wrap(timeslice->size_component(i));

    // Rewrite the offset in the timeslice component descriptor.
    auto* tscd = timeslice->desc_ptr_[i];
    tscd->offset = data_.at(i).write_index();

    // Copy the data into the shared memory.
    data_.at(i).append(timeslice->data_ptr_[i], timeslice->size_component(i));
    desc_.at(i).append(tscd, 1);
  }

  // Rewrite the timeslice index in the descriptor
  auto tsd = timeslice->timeslice_descriptor_;
  tsd.ts_pos = ts_pos_++;

  // Send the work item.
  timeslice_buffer_.send_work_item({tsd, timeslice_buffer_.get_data_size_exp(),
                                    timeslice_buffer_.get_desc_size_exp()});
}
