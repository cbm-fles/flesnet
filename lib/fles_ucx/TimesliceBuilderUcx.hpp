// Copyright 2013-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeConnectionUcx.hpp"
#include "IBConnectionGroupUcx.hpp"
#include "Monitor.hpp"
#include "RingBuffer.hpp"
#include "TimesliceBuffer.hpp"
#include <csignal>
#include <vector>

/// Timeslice receiver and input node connection container class.
/** A TimesliceBuilder object represents a group of timeslice building
 connections to input nodes and receives timeslices to a timeslice buffer. */

class TimesliceBuilderUcx
    : public IBConnectionGroupUcx<ComputeNodeConnectionUcx> {
public:
  /// The TimesliceBuilder constructor.
  TimesliceBuilderUcx(uint64_t compute_index,
                      TimesliceBuffer& timeslice_buffer,
                      unsigned short service,
                      uint32_t num_input_nodes,
                      uint32_t timeslice_size,
                      volatile sig_atomic_t* signal_status,
                      bool drop,
                      cbm::Monitor* monitor);

  TimesliceBuilderUcx(const TimesliceBuilderUcx&) = delete;
  void operator=(const TimesliceBuilderUcx&) = delete;

  /// The TimesliceBuilder destructor.
  ~TimesliceBuilderUcx() override;

  void report_status();

  void request_abort();

  void operator()() override;

  [[nodiscard]] std::string thread_name() const override {
    return "TSB/RDMA/o" + std::to_string(compute_index_);
  };

  /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
  void on_connect_request(struct rdma_cm_event* event) override;

  /// Completion notification event dispatcher. Called by the event loop.
  void on_completion(const struct ibv_wc& wc) override;

  void poll_ts_completion();

private:
  uint64_t compute_index_;
  TimesliceBuffer& timeslice_buffer_;

  unsigned short service_;
  uint32_t num_input_nodes_;

  uint32_t timeslice_size_;

  size_t red_lantern_ = 0;
  uint64_t completely_written_ = 0;
  uint64_t acked_ = 0;

  /// Buffer to store acknowledged status of timeslices.
  RingBuffer<uint64_t, true> ack_;

  volatile sig_atomic_t* signal_status_;
  bool drop_;

  std::vector<ComputeNodeConnectionUcx::BufferStatus>
      previous_recv_buffer_status_desc_;
  std::vector<ComputeNodeConnectionUcx::BufferStatus>
      previous_recv_buffer_status_data_;

  cbm::Monitor* monitor_;
  std::string hostname_;
};
