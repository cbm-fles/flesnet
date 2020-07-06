// Copyright 2019 Farouk Salem <salem@zib.de>

#pragma once

#include "HeartbeatManager.hpp"

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
class InputHeartbeatManager : public HeartbeatManager {
public:
  // Initialize the instance and retrieve it
  static InputHeartbeatManager* get_instance(uint32_t index,
                                             uint32_t init_connection_count,
                                             std::string log_directory,
                                             bool enable_logging);

  // Get singleton instance
  static InputHeartbeatManager* get_instance();

  // Set the begin time to be used in logging
  void log_heartbeat(uint32_t connection_id);

  // Upon receiving a sync message, the latency is calculated and used
  // accordingly
  void log_new_latency(uint32_t connection_id, uint64_t latency);

  // Retrieve the inactive connections to send heartbeat message
  std::vector<uint32_t> retrieve_new_inactive_connections();

  // Retrieve a list of timeout connections
  const std::set<uint32_t> retrieve_timeout_connections();

  // Get a new timed out connection to send heartbeat message (-1 is returned
  // if there is no)
  int32_t get_new_timeout_connection();

  // Check whether a connection is inactive
  bool is_connection_inactive(uint32_t connection_id);

  // Check whether a connection is already timedout
  bool is_connection_timed_out(uint32_t connection_id);

  // Mark connection as timedout
  HeartbeatFailedNodeInfo* mark_connection_timed_out(
      uint32_t connection_id, uint64_t last_desc, uint64_t timeslice_trigger);

  // Get the first timeslice trigger to next send TSs after it
  uint64_t get_timeslice_blockage_trigger();

  // Remove timeslice trigger of connection after decision arrival
  bool remove_timeslice_blockage_trigger(uint32_t connection_id);

  // Get the number of active connections
  uint32_t get_active_connection_count();

  // Get the number of timeout connections
  uint32_t get_timeout_connection_count();

  // Generate log files of the stored data
  // void generate_log_files();

private:
  struct ConnectionHeartbeatInfo {
    std::chrono::high_resolution_clock::time_point last_received_message =
        std::chrono::high_resolution_clock::now();
    std::vector<uint64_t> latency_history; // in microseconds
    uint64_t sum_latency = ConstVariables::HEARTBEAT_TIMEOUT_HISTORY_SIZE *
                           ConstVariables::HEARTBEAT_TIMEOUT;
    uint32_t next_latency_index = 0;
  };

  InputHeartbeatManager(uint32_t index,
                        uint32_t init_connection_count,
                        std::string log_directory,
                        bool enable_logging);

  // Check whether a connection should be inactive
  bool check_whether_connection_inactive(uint32_t connection_id);

  // Check whether a connection should be timed out
  bool check_whether_connection_timed_out(uint32_t connection_id);

  // The singleton instance for this class
  static InputHeartbeatManager* instance_;

  // Time of the last received message from a connection
  std::vector<ConnectionHeartbeatInfo*> connection_heartbeat_time_;

  // List of pending timed-out connections <trigger, conn>
  std::vector<std::pair<uint64_t, uint32_t>> pending_timed_out_connection_;

  // List of the inactive connections
  std::set<uint32_t> inactive_connection_;

  // Failed connection index and its corresponding HeartbeatFailedNodeInfo
  SizedMap<uint32_t, HeartbeatFailedNodeInfo*> timeout_node_info_;
};
} // namespace tl_libfabric
