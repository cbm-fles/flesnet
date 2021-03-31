// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>
#pragma once

#include "ChildProcessManager.hpp"
#include "ComputeNodeConnection.hpp"
#include "ConnectionGroup.hpp"
#include "RequestIdentifier.hpp"
#include "RingBuffer.hpp"
#include "TimesliceBuffer.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceWorkItem.hpp"
#include "dfs/DDSchedulerOrchestrator.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <csignal>
#include <cstdint>
#include <iomanip>
#include <map>
#include <set>
#include <string>

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
                   std::string local_node_name,
                   uint32_t scheduler_history_size,
                   uint32_t scheduler_interval_length,
                   uint32_t scheduler_speedup_difference_percentage,
                   uint32_t scheduler_speedup_percentage,
                   uint32_t scheduler_speedup_interval_count,
                   std::string log_directory,
                   bool enable_logging);

  TimesliceBuilder(const TimesliceBuilder&) = delete;
  void operator=(const TimesliceBuilder&) = delete;

  /// The ComputeBuffer destructor.
  ~TimesliceBuilder() override;

  void report_status();

  void request_abort();

  void operator()() override;

  void sync_heartbeat() override;

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

  /// Process completed timeslices and send them to the analyzer
  void process_completed_timeslices();

  /// Check if a timeslice is received completely
  bool check_complete_timeslices(uint64_t ts_pos);

  /// Mark connection as completed in case of normal termination or failure
  void mark_connection_completed(uint32_t conn_id);

  // Finalize a connection after getting the finalize completion event
  void on_send_finalize_event(uint32_t conn_id);

  // Check hearbeat message content
  void on_recv_heartbeat_event(uint32_t conn_id);

  // Check the missing failure info from input nodes
  void check_missing_connections_failure_info();

  // Check whether a finalize connection is sent and no response is received
  void check_long_waiting_finalized_connections();

  // Check and send heartbeats to inactive connections
  void check_inactive_connections();

  void build_time_file();

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

  uint64_t completely_written_ = 0;
  uint64_t acked_ = 0;

  /// Buffer to store acknowledged status of timeslices.
  RingBuffer<uint64_t, true> ack_;

  volatile sig_atomic_t* signal_status_;

  std::string local_node_name_;

  bool drop_;

  // LOGGING
  std::map<uint64_t, double> first_last_arrival_diff_;
  std::map<uint64_t, std::chrono::high_resolution_clock::time_point>
      first_arrival_time_;
  std::map<uint64_t, uint32_t> arrival_count_;
  std::map<uint64_t, std::vector<double>> buffer_status_;
  std::string log_directory_;
  // END OF LOGGING
};
} // namespace tl_libfabric
