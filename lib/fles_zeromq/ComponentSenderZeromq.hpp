// Copyright 2012-2013, 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DualRingBuffer.hpp"
#include "Monitor.hpp"
#include "RingBuffer.hpp"
#include "Scheduler.hpp"
#include <boost/format.hpp>
#include <cassert>
#include <csignal>
#include <mutex>
#include <zmq.h>

/// Input buffer and compute node connection container class.
/** An ComponentSenderZeromq object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class ComponentSenderZeromq {
public:
  /// The ComponentSenderZeromq default constructor.
  ComponentSenderZeromq(uint64_t input_index,
                        InputBufferReadInterface& data_source,
                        const std::string& listen_address,
                        uint32_t timeslice_size,
                        uint32_t overlap_size,
                        uint32_t max_timeslice_number,
                        volatile sig_atomic_t* signal_status,
                        void* zmq_context,
                        cbm::Monitor* monitor);

  ComponentSenderZeromq(const ComponentSenderZeromq&) = delete;
  void operator=(const ComponentSenderZeromq&) = delete;

  /// The ComponentSenderZeromq default destructor.
  ~ComponentSenderZeromq();

  /// The thread main function.
  void operator()();

  /**
   * @brief Return a text description of the object (to be used as a thread
   * name).
   *
   * @return A string describing the object (at most 15 characters long).
   */
  [[nodiscard]] std::string thread_name() const {
    return "CS/ZMQ/i" + std::to_string(input_index_);
  };

  friend void enqueue_ack(void* data, void* hint);

private:
  /// This component's index in the list of input components.
  uint64_t input_index_;

  /// Data source (e.g., FLIB via shared memory).
  InputBufferReadInterface& data_source_;

  /// Constant size (in microslices) of a timeslice component.
  const uint32_t timeslice_size_;

  /// Constant overlap size (in microslices) of a timeslice component.
  const uint32_t overlap_size_;

  /// Number of timeslices after which this run shall end.
  const uint32_t max_timeslice_number_;

  /// Pointer to global signal status variable.
  volatile sig_atomic_t* signal_status_;

  /// ZeroMQ socket.
  void* socket_;

  struct Acknowledgment {
    ComponentSenderZeromq* server;
    uint64_t timeslice;
    bool is_data;
  };

  /// Queue to store pending acknowledgments.
  std::queue<Acknowledgment*> pending_acks_;

  /// Mutex to manage access to the pending_acks_ queue.
  std::mutex pending_acks_mutex_;

  /// Buffer to store acknowledged status of timeslices.
  RingBuffer<uint64_t, true> ack_;

  /// Number of acknowledged timeslices (times two - desc and data).
  uint64_t acked_ts2_ = 0;

  /// Indexes of acknowledged microslices (i.e., read indexes).
  DualIndex acked_{};

  /// Hysteresis for writing read indexes to data source.
  const DualIndex min_acked_;

  /// Read indexes last written to data source.
  DualIndex cached_acked_{};

  /// Read indexes at start of operation.
  DualIndex start_index_{};

  /// Write index received from data source.
  uint64_t write_index_desc_ = 0;

  /// Begin of operation (for performance statistics).
  std::chrono::high_resolution_clock::time_point time_begin_;

  /// End of operation (for performance statistics).
  std::chrono::high_resolution_clock::time_point time_end_;

  /// Amount of data sent (for performance statistics).
  DualIndex sent_{};

  cbm::Monitor* monitor_;
  std::string hostname_;

  struct SendBufferStatus {
    std::chrono::system_clock::time_point time;
    uint64_t size = 0;

    uint64_t cached_acked = 0;
    uint64_t acked = 0;
    uint64_t sent = 0;
    uint64_t written = 0;

    [[nodiscard]] int64_t used() const {
      assert(sent <= written);
      return written - sent;
    }
    [[nodiscard]] int64_t sending() const {
      assert(acked <= sent);
      return sent - acked;
    }
    [[nodiscard]] int64_t freeing() const {
      assert(cached_acked <= acked);
      return acked - cached_acked;
    }
    [[nodiscard]] int64_t unused() const {
      assert(written <= cached_acked + size);
      return cached_acked + size - written;
    }

    [[nodiscard]] float percentage(int64_t value) const {
      return static_cast<float>(value) / static_cast<float>(size);
    }

    static std::string caption() { return {"used/sending/freeing/free"}; }

    [[nodiscard]] std::string percentage_str(int64_t value) const {
      boost::format percent_fmt("%4.1f%%");
      percent_fmt % (percentage(value) * 100);
      std::string s = percent_fmt.str();
      s.resize(4);
      return s;
    }

    [[nodiscard]] std::string percentages() const {
      return percentage_str(used()) + " " + percentage_str(sending()) + " " +
             percentage_str(freeing()) + " " + percentage_str(unused());
    }

    [[nodiscard]] std::vector<int64_t> vector() const {
      return std::vector<int64_t>{used(), sending(), freeing(), unused()};
    }
  };

  SendBufferStatus previous_send_buffer_status_desc_ = SendBufferStatus();
  SendBufferStatus previous_send_buffer_status_data_ = SendBufferStatus();

  /// Scheduler for periodic events.
  Scheduler scheduler_;

  /// Setup at begin of run.
  void run_begin();

  /// A single cycle in the main run loop.
  bool run_cycle();

  /// Cleanup at end of run.
  void run_end();

  /// Process the pending acknowledgments received from a ZeroMQ thread.
  void process_pending_acks();

  /// The central function for distributing timeslice data.
  bool try_send_timeslice(uint64_t ts);

  /// Create zeromq message part with requested data.
  template <typename T_>
  zmq_msg_t create_message(RingBufferView<T_>& buf,
                           uint64_t offset,
                           uint64_t length,
                           uint64_t ts,
                           bool is_data);

  /// Update read indexes after timeslice has been sent.
  void ack_timeslice(uint64_t ts, bool is_data);

  /// Force writing read indexes to data source.
  void sync_data_source();

  /// Print a (periodic) buffer status report.
  void report_status();
};
