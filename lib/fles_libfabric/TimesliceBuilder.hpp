// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>
#pragma once

#include "ComputeNodeConnection.hpp"
#include "ConnectionGroup.hpp"
#include "RingBuffer.hpp"
#include "TimesliceComponentDescriptor.hpp"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <csignal>
#include <cstdint>
#include <set>
#include <string>

#include "ComputeNodeConnection.hpp"
#include "ConnectionGroup.hpp"
#include "TimesliceBuffer.hpp"

namespace tl_libfabric {

/// Compute buffer and input node connection container class.
/** A ComputeBuffer object represents a timeslice buffer (filled by
 the input nodes) and a group of timeslice building connections to
 input nodes. */

class TimesliceBuilder : public ConnectionGroup<ComputeNodeConnection> {
public:
  /// The ComputeBuffer constructor.
  TimesliceBuilder(uint64_t compute_index,
                   TimesliceBuffer& timeslice_buffer,
                   unsigned short service,
                   uint32_t num_input_nodes,
                   uint32_t timeslice_size,
                   volatile sig_atomic_t* signal_status,
                   bool drop,
                   const std::string& local_node_name);

  TimesliceBuilder(const TimesliceBuilder&) = delete;
  void operator=(const TimesliceBuilder&) = delete;

  /// The ComputeBuffer destructor.
  ~TimesliceBuilder() override;

  void report_status();

  void request_abort();

  void operator()() override;

  /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
  void on_connect_request(struct fi_eq_cm_entry* event,
                          size_t private_data_len) override;

  /// Completion notification event dispatcher. Called by the event loop.
  void on_completion(uint64_t wr_id) override;

  void poll_ts_completion();

private:
  /// setup connections between nodes
  void bootstrap_with_connections();

  /// setup connections between nodes
  void bootstrap_wo_connections();

  void make_endpoint_named(struct fi_info* info,
                           const std::string& hostname,
                           const std::string& service,
                           struct fid_ep** ep);

  fid_cq* listening_cq_;
  uint64_t compute_index_;

  // used in connection-less mode
  InputChannelStatusMessage recv_connect_message_ = InputChannelStatusMessage();

  struct fid_mr* mr_recv_ = nullptr;

  TimesliceBuffer& timeslice_buffer_;

  unsigned short service_;
  uint32_t num_input_nodes_;

  std::set<uint_fast16_t> connected_senders_;

  uint32_t timeslice_size_;

  size_t red_lantern_ = 0;
  uint64_t completely_written_ = 0;
  uint64_t acked_ = 0;

  /// Buffer to store acknowledged status of timeslices.
  RingBuffer<uint64_t, true> ack_;

  volatile sig_atomic_t* signal_status_;

  std::string local_node_name_;

  bool drop_;
};
} // namespace tl_libfabric
