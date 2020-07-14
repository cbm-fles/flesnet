// Copyright 2019 Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
#include "HeartbeatFailedNodeInfo.hpp"
#include "HeartbeatMessage.hpp"
#include "SizedMap.hpp"

#include <cassert>
#include <chrono>
#include <log.hpp>
#include <math.h>
#include <set>
#include <string>

#include <fstream>
#include <iomanip>
#include <vector>

namespace tl_libfabric {
/**
 * Singleton Heartbeat manager that DFS uses to detect the failure of a
 * connection
 */
class HeartbeatManager {
public:
  // Log sent heartbeat message
  void log_sent_heartbeat_message(uint32_t connection_id,
                                  HeartbeatMessage message);

  // get next message id sequence
  uint64_t get_next_heartbeat_message_id();

  // Log the time that the heartbeat message is acked
  void ack_message_received(uint64_t messsage_id);

  // Count the unacked messages of a particular connection
  uint32_t count_unacked_messages(uint32_t connection_id);

  // Add new pending messages
  void add_pending_message(uint32_t connection_id, HeartbeatMessage* message);

  // Get one of the pending messages, if there
  HeartbeatMessage* get_pending_message(uint32_t connection_id);

  // Remove all pending messages of a connection when it times out
  void clear_pending_messages(uint32_t connection_id);

  // Set the begin time to be used in logging
  void log_heartbeat(uint32_t connection_id);

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

  // Get the number of active connections
  uint32_t get_active_connection_count();

  // Get the number of timeout connections
  uint32_t get_timeout_connection_count();

  // Mark connection as timedout
  HeartbeatFailedNodeInfo* mark_connection_timed_out(
      uint32_t connection_id, uint64_t last_desc, uint64_t timeslice_trigger);

protected:
  struct HeartbeatMessageInfo {
    HeartbeatMessage message;
    std::chrono::high_resolution_clock::time_point transmit_time;
    std::chrono::high_resolution_clock::time_point completion_time;
    bool acked = false;
    uint32_t dest_connection;
  };

  struct ConnectionHeartbeatInfo {
    std::chrono::high_resolution_clock::time_point last_received_message =
        std::chrono::high_resolution_clock::now();
    std::vector<uint64_t> latency_history; // in microseconds
    uint64_t sum_latency;
    uint32_t next_latency_index = 0;
    ConnectionHeartbeatInfo(uint64_t init_sum_latency) {
      sum_latency = init_sum_latency;
    }
  };

  HeartbeatManager(uint32_t index,
                   uint32_t init_connection_count,
                   uint64_t init_heartbeat_timeout,
                   uint32_t timeout_history_size,
                   uint32_t timeout_factor,
                   uint32_t inactive_factor,
                   uint32_t inactive_retry_count,
                   std::string log_directory,
                   bool enable_logging);

  // Check whether a connection should be inactive
  bool check_whether_connection_inactive(uint32_t connection_id);

  // Check whether a connection should be timed out
  bool check_whether_connection_timed_out(uint32_t connection_id);

  // Upon receiving a sync message, the latency is calculated and used
  // accordingly
  void log_new_latency(uint32_t connection_id, uint64_t latency);

  // Compute process index
  uint32_t index_;

  // The number of input connections
  uint32_t connection_count_;

  // Sent message log <message_id, message_info>
  SizedMap<uint64_t, HeartbeatMessageInfo*> heartbeat_message_log_;

  // Not acked messages log
  SizedMap<uint32_t, std::set<uint64_t>*> unacked_sent_messages_;

  // Pending messages to be sent
  std::vector<std::vector<HeartbeatMessage*>> pending_messages_;

  // Time of the last received message from a connection
  std::vector<ConnectionHeartbeatInfo*> connection_heartbeat_time_;

  // List of pending timed-out connections <trigger, conn>
  std::vector<std::pair<uint64_t, uint32_t>> pending_timed_out_connection_;

  // List of the inactive connections
  std::set<uint32_t> inactive_connection_;

  // Failed connection index and its corresponding HeartbeatFailedNodeInfo
  SizedMap<uint32_t, HeartbeatFailedNodeInfo*> timeout_node_info_;

  uint64_t init_heartbeat_timeout_;

  // history size of latency to detect timeout connections
  // high for unstable networks
  uint32_t timeout_history_size_;

  // # of timeout value before considering a connection timed out
  uint32_t timeout_factor_;

  // # of timeout messages before considering a connection is inactive
  uint32_t inactive_factor_;

  uint32_t inactive_retry_count_;

  // The log directory
  std::string log_directory_;

  bool enable_logging_;

  bool update_latency_log_;
};
} // namespace tl_libfabric
