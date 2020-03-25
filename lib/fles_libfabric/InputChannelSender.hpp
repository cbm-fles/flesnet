// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include "ConnectionGroup.hpp"
#include "DualRingBuffer.hpp"
#include "InputChannelConnection.hpp"
#include "RingBuffer.hpp"
#include <boost/format.hpp>
#include <cassert>

#include <rdma/fi_domain.h>
#include <set>
#include <string>
#include <vector>

namespace tl_libfabric {
/// Input buffer and compute node connection container class.
/** An InputChannelSender object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class InputChannelSender : public ConnectionGroup<InputChannelConnection> {
public:
  /// The InputChannelSender default constructor.
  InputChannelSender(uint64_t input_index,
                     InputBufferReadInterface& data_source,
                     const std::vector<std::string>& compute_hostnames,
                     const std::vector<std::string>& compute_services,
                     uint32_t timeslice_size,
                     uint32_t overlap_size,
                     uint32_t max_timeslice_number,
                     const std::string& input_node_name);

  InputChannelSender(const InputChannelSender&) = delete;
  void operator=(const InputChannelSender&) = delete;

  /// The InputChannelSender default destructor.
  ~InputChannelSender() override;

  void report_status();

  void sync_buffer_positions();
  void sync_data_source(bool schedule);

  void operator()() override;

  /// The central function for distributing timeslice data.
  bool try_send_timeslice(uint64_t timeslice);

  std::unique_ptr<InputChannelConnection>
  create_input_node_connection(uint_fast16_t index);

  /// Initiate connection requests to list of target hostnames.
  void connect();

  void on_connected(struct fid_domain* pd) override;

private:
  /// Return target computation node for given timeslice.
  int target_cn_index(uint64_t timeslice);

  /// Handle RDMA_CM_REJECTED event.
  void on_rejected(struct fi_eq_err_entry* event) override;

  /// Return string describing buffer contents, suitable for debug output.
  std::string get_state_string();

  /// Create gather list for transmission of timeslice
  void post_send_data(uint64_t timeslice,
                      int cn,
                      uint64_t desc_offset,
                      uint64_t desc_length,
                      uint64_t data_offset,
                      uint64_t data_length,
                      uint64_t skip);

  /// Completion notification event dispatcher. Called by the event loop.
  void on_completion(uint64_t wr_id) override;

  /// setup connections between nodes
  void bootstrap_with_connections();

  /// setup connections between nodes
  void bootstrap_wo_connections();

  uint64_t input_index_;

  /// Libfabric memory region descriptor for input data buffer.
  struct fid_mr* mr_data_ = nullptr;

  /// Libfabric memory region descriptor for input descriptor buffer.
  struct fid_mr* mr_desc_ = nullptr;

  /// Buffer to store acknowledged status of timeslices.
  RingBuffer<uint64_t, true> ack_;

  /// Number of acknowledged microslices. Written to FLIB.
  uint64_t acked_desc_ = 0;

  /// Number of acknowledged data bytes. Written to FLIB.
  uint64_t acked_data_ = 0;

  /// Data source (e.g., FLIB).
  InputBufferReadInterface& data_source_;

  /// Number of sent microslices, for statistics.
  uint64_t sent_desc_ = 0;

  /// Number of sent data bytes, for statistics.
  uint64_t sent_data_ = 0;

  const std::vector<std::string> compute_hostnames_;
  const std::vector<std::string> compute_services_;

  const uint32_t timeslice_size_;
  const uint32_t overlap_size_;
  const uint32_t max_timeslice_number_;

  const uint64_t min_acked_desc_;
  const uint64_t min_acked_data_;

  uint64_t cached_acked_data_ = 0;
  uint64_t cached_acked_desc_ = 0;

  uint64_t start_index_desc_;
  uint64_t start_index_data_;

  uint64_t write_index_desc_ = 0;

  std::set<uint_fast16_t> connected_buffers_;

  bool abort_ = false;

  struct SendBufferStatus {
    std::chrono::system_clock::time_point time;
    uint64_t size;

    uint64_t cached_acked;
    uint64_t acked;
    uint64_t sent;
    uint64_t written;

    int64_t used() const {
      assert(sent <= written);
      return written - sent;
    }
    int64_t sending() const {
      assert(acked <= sent);
      return sent - acked;
    }
    int64_t freeing() const {
      assert(cached_acked <= acked);
      return acked - cached_acked;
    }
    int64_t unused() const {
      assert(written <= cached_acked + size);
      return cached_acked + size - written;
    }

    float percentage(int64_t value) const {
      return static_cast<float>(value) / static_cast<float>(size);
    }

    static std::string caption() {
      return std::string("used/sending/freeing/free");
    }

    std::string percentage_str(int64_t value) const {
      boost::format percent_fmt("%4.1f%%");
      percent_fmt % (percentage(value) * 100);
      std::string s = percent_fmt.str();
      s.resize(4);
      return s;
    }

    std::string percentages() const {
      return percentage_str(used()) + " " + percentage_str(sending()) + " " +
             percentage_str(freeing()) + " " + percentage_str(unused());
    }

    std::vector<int64_t> vector() const {
      return std::vector<int64_t>{used(), sending(), freeing(), unused()};
    }
  };

  SendBufferStatus previous_send_buffer_status_desc_ = SendBufferStatus();
  SendBufferStatus previous_send_buffer_status_data_ = SendBufferStatus();
};
} // namespace tl_libfabric
