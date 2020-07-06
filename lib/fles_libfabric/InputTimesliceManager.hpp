// Copyright 2018 Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
#include "HeartbeatFailedNodeInfo.hpp"
#include "SizedMap.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <log.hpp>
#include <map>
#include <math.h>
#include <set>
#include <string>
#include <vector>

namespace tl_libfabric {
/**
 * Singleton Input Timeslice Manager for input nodes that could be used in
 * InputChannelSender and InputChannelConnections!
 */
class InputTimesliceManager {
public:
  // Initialize and get singleton instance
  static InputTimesliceManager* get_instance(uint32_t scheduler_index,
                                             uint32_t compute_conn_count,
                                             uint32_t interval_length,
                                             uint64_t data_source_desc,
                                             uint64_t desc_length,
                                             uint64_t start_index_desc,
                                             uint64_t timeslice_size,
                                             std::string log_directory,
                                             bool enable_logging);

  // Get singleton instance
  static InputTimesliceManager* get_instance();

  // Get the next timeslice to be trasmitted to specific compute node
  uint64_t get_connection_next_timeslice(uint32_t compute_index);

  // Log the transmission time of a timeslice
  void log_timeslice_transmit_time(uint32_t compute_index,
                                   uint64_t timeslice,
                                   uint64_t size);

  // log the duration to write a timeslice to the destination
  bool acknowledge_timeslice_rdma_write(uint32_t compute_index,
                                        uint64_t timeslice);

  // mark the timeslices up to specific descriptor as completed the acked
  // timeslices by one and returns the average latency of these timeslices
  double acknowledge_timeslices_completion(uint32_t compute_index,
                                           uint64_t up_to_descriptor_id);

  // Check whether a timeslice is acked
  bool is_timeslice_rdma_acked(uint32_t compute_index, uint64_t timeslice);

  // Check whether a timeslice belongs to a timeout connection
  bool is_timeslice_belongs_to_timeout_connection(
      uint64_t timeslice, const std::set<uint32_t> timeout_connections);

  // Get the number of current compute node connections
  uint32_t get_compute_connection_count();

  // Get the last acked descriptor ID of a compute node
  uint64_t get_last_acked_descriptor(uint32_t compute_index);

  // Get the timeslice number of a specific descriptor
  uint64_t get_timeslice_by_descriptor(uint32_t compute_index,
                                       uint64_t descriptor);

  // Get the timeslice number of the last RDMA acked
  uint64_t get_last_rdma_acked_timeslice(uint32_t compute_index);

  // Calculate the last possible timeslice to be sent before blockage when a
  // connection timed out
  uint64_t get_last_timeslice_before_blockage(
      const uint32_t failed_compute_index,
      const std::set<uint32_t> timeout_connections,
      const uint64_t current_interval_start_ts,
      const uint64_t current_interval_end_ts);

  // Get last descriptor of a connection
  uint64_t get_last_connection_descriptor_index(uint32_t compute_index);

  // Get the count of timeslices of an interval
  uint64_t count_timeslices_of_interval(uint32_t compute_index,
                                        uint64_t start_ts,
                                        uint64_t end_ts);

  // Get the count of un-acked but sent timeslices of an interval of a
  // particular compute node
  uint64_t count_unacked_timeslices_of_interval(uint32_t compute_index,
                                                uint64_t start_ts,
                                                uint64_t end_ts);

  // Get the count of future timeslices of an interval of a particular compute
  // node
  uint64_t count_future_timeslices_of_interval(uint32_t compute_index,
                                               uint64_t start_ts,
                                               uint64_t end_ts);

  // Return the data size and the descriptor index of a timeslice
  std::pair<uint64_t, uint64_t>
  get_data_and_desc_of_timeslice(uint32_t compute_index, uint32_t timeslice);

  // Return the data size and the descriptor index of the last timeslice
  std::pair<uint64_t, uint64_t>
  get_data_and_desc_of_last_timeslice(uint32_t compute_index);

  // Inform the TimesliceManager about the decision of the failed node and
  // returns the rescheduled transmitted timeslices
  std::vector<uint64_t>
  consider_reschedule_decision(HeartbeatFailedNodeInfo failed_node_info,
                               const std::set<uint32_t> timeout_connections);

  // Generate log files of the stored data
  void generate_log_files();

  uint64_t log_timeslice_IB_blocked(uint64_t timeslice,
                                    bool sent_completed = false);

  uint64_t log_timeslice_CB_blocked(uint64_t timeslice,
                                    bool sent_completed = false);

  uint64_t log_timeslice_MR_blocked(uint64_t timeslice,
                                    bool sent_completed = false);

  bool is_decision_considered(uint32_t connection_id) {
    return redistribution_decisions_log_.contains(connection_id);
  }

  bool get_failure_decision(uint32_t connection_id) {
    return redistribution_decisions_log_.contains(connection_id);
  }

  void update_data_source_desc(uint64_t data_source_desc) {
    data_source_desc_ = data_source_desc;
  }

  uint64_t get_sent_timeslices() { return sent_timeslices_; }

private:
  struct TimesliceInfo {
    std::chrono::high_resolution_clock::time_point transmit_time;
    uint64_t data;
    uint64_t compute_desc;
    uint64_t rdma_acked_duration = 0;
    uint64_t completion_acked_duration = 0;
  };

  InputTimesliceManager(uint32_t scheduler_index,
                        uint32_t compute_conn_count,
                        uint32_t interval_length,
                        uint64_t data_source_desc,
                        uint64_t desc_length,
                        uint64_t start_index_desc,
                        uint64_t timeslice_size,
                        std::string log_directory,
                        bool enable_logging);

  // Refill the array of the future timeslices of each connection (considering
  // timeout connections)
  void refill_future_timeslices(uint64_t up_to_timeslice);

  // Insert rescheduled timeslices after the correct timeslice
  void check_to_add_rescheduled_timeslices(uint32_t compute_index);

  // Mark the transmitted timeslices after the trigger as unsent and returns
  // these timeslices
  std::vector<uint64_t>
  undo_transmitted_timeslices_after_trigger(uint64_t timeslice_trigger);

  // Refill the future timeslices or return back the sent ones to be for
  // future
  std::vector<uint64_t> manage_after_trigger_timeslices_on_failure(
      HeartbeatFailedNodeInfo failed_node_info);

  // Retrieve the timeslices of the failed node
  std::vector<uint64_t> retrieve_failed_timeslices_on_failure(
      HeartbeatFailedNodeInfo failed_node_info);

  // distribute the failed timeslices on the active compute connections
  void distribute_failed_timeslices_on_active_connections(
      HeartbeatFailedNodeInfo failed_node_info,
      const std::set<uint32_t> timeout_connections);

  // The singleton instance for this class
  static InputTimesliceManager* instance_;

  // The number of compute connections
  uint32_t compute_count_;

  // #virtual compute connections according to scheduler frequency
  uint32_t virtual_compute_count_;

  //
  std::vector<uint32_t> virtual_physical_compute_mapping_;

  // Input Scheduler index
  uint32_t scheduler_index_;

  // Number of intial timeslices per interval
  uint32_t interval_length_;

  // latest & updated data source desc
  uint64_t data_source_desc_;

  //
  uint64_t desc_length_;

  //
  uint64_t start_index_desc_;

  //
  uint64_t timeslice_size_;

  // The count of sent timeslices
  uint64_t sent_timeslices_ = 0;

  // The Log folder
  std::string log_directory_;

  // Check whether to generate log files
  bool enable_logging_;

  // Mapping of connections and their transmitted timeslices
  SizedMap<uint32_t, SizedMap<uint64_t, TimesliceInfo*>*> conn_timeslice_info_;

  // Mapping of timeslice and its descriptor ID of each compute node
  SizedMap<uint32_t, SizedMap<uint64_t, uint64_t>*> conn_desc_timeslice_info_;

  // Future Timeslices for each compute connection <conn_id, <timeslices>>
  SizedMap<uint32_t, std::set<uint64_t>*> future_conn_timeslices_;

  // The received decisions of redistribution
  SizedMap<uint32_t, uint64_t> redistribution_decisions_log_;

  // Temp list for the "TO BE MOVED timeslices" key: trigger timeslice, values
  // a vector of connections and its share of timeslices
  SizedMap<uint64_t, std::vector<std::set<uint64_t>*>> to_be_moved_timeslices_;

  // Last transmitted desc of each connection
  std::vector<uint64_t> last_conn_desc_;
  std::vector<uint64_t> last_conn_timeslice_;

  // Indicator of the timeslice, to start from, to refill the
  // future_conn_timeslices_ list
  uint64_t next_start_future_timeslice_ = ConstVariables::ZERO;

  /// LOGGING
  // Input Buffer blockage
  SizedMap<uint64_t, std::chrono::high_resolution_clock::time_point>
      timeslice_IB_blocked_start_log_;
  SizedMap<uint64_t, uint64_t> timeslice_IB_blocked_duration_log_;
  // Compute Buffer blockage
  SizedMap<uint64_t, std::chrono::high_resolution_clock::time_point>
      timeslice_CB_blocked_start_log_;
  SizedMap<uint64_t, uint64_t> timeslice_CB_blocked_duration_log_;
  // Max writes limitation blockage
  SizedMap<uint64_t, std::chrono::high_resolution_clock::time_point>
      timeslice_MR_blocked_start_log_;
  SizedMap<uint64_t, uint64_t> timeslice_MR_blocked_duration_log_;
};
} // namespace tl_libfabric
