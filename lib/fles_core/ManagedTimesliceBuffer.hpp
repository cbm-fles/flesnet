// Copyright 2023 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ItemDistributor.hpp"
#include "ManagedRingBuffer.hpp"
#include "RingBuffer.hpp"
#include "Sink.hpp"
#include "TimesliceBuffer.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <thread>
#include <vector>
#include <zmq.h>

/**
 * \brief The ManagedTimesliceBuffer manages the items in a shared memory
 * TimesliceBuffer. It implements the TimesliceSink interface to receive
 * Timeslice objects.
 */
class ManagedTimesliceBuffer : public fles::TimesliceSink {
public:
  /// The ManagedTimesliceBuffer constructor.
  ManagedTimesliceBuffer(zmq::context_t& context,
                         const std::string& shm_identifier,
                         uint32_t data_buffer_size_exp,
                         uint32_t desc_buffer_size_exp,
                         uint32_t num_input_nodes);

  /// The ManagedTimesliceBuffer destructor.
  ~ManagedTimesliceBuffer() override;

  ManagedTimesliceBuffer(const ManagedTimesliceBuffer&) = delete;
  void operator=(const ManagedTimesliceBuffer&) = delete;

  void put(std::shared_ptr<const fles::Timeslice> timeslice) override;

private:
  /// Address that is used for communication between the TimesliceBuffer and the
  /// ItemDistributor.
  const std::string producer_address_;

  /// Address that is used by Workers to connect to the ItemDistributor.
  const std::string worker_address_;

  /// The ItemDistributor object.
  ItemDistributor item_distributor_;

  /// Shared memory buffer to store received timeslices.
  TimesliceBuffer timeslice_buffer_;

  /// Buffer to store acknowledged status of timeslices.
  RingBuffer<uint64_t, true> ack_;

  /// Thread for the ItemDistributor.
  std::thread distributor_thread_;

  /// The index of acknowledged timeslices (local buffer position).
  uint64_t acked_ = 0;

  /// The index of the timeslice currently being received (local buffer
  /// position).
  uint64_t ts_pos_ = 0;

  /// ManagedRingBuffer wrappers for the TimesliceComponentDescriptor buffer.
  std::vector<ManagedRingBuffer<fles::TimesliceComponentDescriptor>> desc_;
  std::vector<ManagedRingBuffer<uint8_t>> data_;

  /// Handle pending timeslice completions and advance read indexes.
  void handle_timeslice_completions();

  /// Check if the timeslice fits in the buffer.
  bool timeslice_fits_in_buffer(const fles::Timeslice& timeslice);
};
