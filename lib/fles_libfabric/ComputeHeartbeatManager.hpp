// Copyright 2019 Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
#include "HeartbeatManager.hpp"
#include "SizedMap.hpp"

#include <cassert>
#include <chrono>
#include <log.hpp>
#include <math.h>
#include <set>
#include <string>
#include <vector>

#include <fstream>
#include <iomanip>

namespace tl_libfabric {
/**
 * Singleton Heart Beat manager that DFS uses to detect the failure of a
 * connection
 */
class ComputeHeartbeatManager : public HeartbeatManager {
public:
  // Initialize the instance and retrieve it
  static ComputeHeartbeatManager* get_instance(uint32_t index,
                                               uint32_t init_connection_count,
                                               std::string log_directory,
                                               bool enable_logging);

  // Get singleton instance
  static ComputeHeartbeatManager* get_instance();

  // log the arrival of failure node message
  HeartbeatFailedNodeInfo*
  log_heartbeat_failure(uint32_t connection_id,
                        HeartbeatFailedNodeInfo failure_info);

  // Check whether a connection is marked as timeout
  bool is_connection_timeout(uint32_t connection_id);

  // Check whether a decision is ready for a failed connection
  bool is_decision_ready(uint32_t failed_connection_id);

  // A list of input connections to inform about a compute node failure
  // <failed node, list of connections>
  std::pair<uint32_t, std::set<uint32_t>>
  retrieve_missing_info_from_connections();

  // Get a decision about a particular failed compute node
  HeartbeatFailedNodeInfo*
  get_decision_of_failed_connection(uint32_t failed_connection_id);

  // Log the acknowledge of receiving a decision
  void log_decision_ack(uint32_t connection_id, uint32_t failed_connection_id);

  // Check whether all decisions are acked
  bool is_all_failure_decisions_acked();

  // Log when the finalize message is sent
  void log_finalize_connection(uint32_t connection_id,
                               bool ack_received = false);

  // Retrieve Connections that are not received any finalize ACK for a timeout
  // period
  std::vector<uint32_t> retrieve_long_waiting_finalized_connections();

  // Get the count of timeout connections
  uint32_t get_timeout_connection_count();

  // Generate log files of the stored data
  // void generate_log_files();

private:
  struct FailureRequestedInfo {
    bool info_requested = false;
    HeartbeatFailedNodeInfo* failure_info = nullptr;
  };

  struct FinalizeConnectionInfo {
    std::chrono::high_resolution_clock::time_point sent_time;
    bool ack_received = false;
  };

  ComputeHeartbeatManager(uint32_t index,
                          uint32_t init_connection_count,
                          std::string log_directory,
                          bool enable_logging);

  // Calculate the final decision of a failure
  void calculate_failure_decision(uint32_t failed_node);

  // The singleton instance for this class
  static ComputeHeartbeatManager* instance_;

  // A list of a failed node and the response of each input node
  SizedMap<uint32_t, std::vector<FailureRequestedInfo*>*>
      collected_failure_info_;

  // The decisions that are still in the collection process <failed node,
  // collected count>
  SizedMap<uint32_t, uint32_t> collected_decisions_count_;

  SizedMap<uint32_t, HeartbeatFailedNodeInfo*> completed_decisions_log_;

  // Temporary acknowledgment of decisions
  SizedMap<uint32_t, std::set<uint32_t>*> decision_ack_log_;

  // Completed acked decisions
  std::set<uint32_t> completed_decision_acked_log_;

  // Monitor the acknowledgment of the ACK messages when finalize message is
  // sent With RxM, sometimes the ack completion events are not received if
  // the remote connection is already closed!
  SizedMap<uint32_t, FinalizeConnectionInfo*> finalize_connection_log_;
};
} // namespace tl_libfabric
