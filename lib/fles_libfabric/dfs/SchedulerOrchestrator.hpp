// Copyright 2020 Farouk Salem <salem@zib.de>

#pragma once

#include "HeartbeatManager.hpp"
#include "HeartbeatMessage.hpp"
#include <cstdint>

namespace tl_libfabric {
/**
 *  Facade layer of common Scheduler functionalities between DDSs and INs
 */
class SchedulerOrchestrator {
public:
  //// Common Methods
  // Initialize
  static void initialize(HeartbeatManager* heartbeat_manager);

  //// ComputeHeartbeatManager Methods

  // Log sent heartbeat message
  static void log_sent_heartbeat_message(std::uint32_t connection_id,
                                         HeartbeatMessage message);

  // get next message id sequence
  static uint64_t get_next_heartbeat_message_id();

  // Acknowledge the arrival of a sent hearbeat message
  static void acknowledge_heartbeat_message(std::uint64_t message_id);

  // Add new pending heartbeat message
  static void add_pending_heartbeat_message(std::uint32_t connection_id,
                                            HeartbeatMessage* message);

  // Get one of the pending messages, if there
  static HeartbeatMessage*
  get_pending_heartbeat_message(std::uint32_t connection_id);

  // Remove all pending messages of a connection when it times out
  static void clear_pending_messages(std::uint32_t connection_id);

  static void log_heartbeat(std::uint32_t connection_id);

  // Retrieve the inactive connections to send heartbeat message
  static std::vector<std::uint32_t> retrieve_new_inactive_connections();

  // Retrieve a list of timeout connections
  static const std::set<std::uint32_t> retrieve_timeout_connections();

  // Get a new timed out connection to send heartbeat message (-1 is returned
  // if there is no)
  static int32_t get_new_timeout_connection();

  // Check whether a connection is already timedout
  static bool is_connection_timed_out(std::uint32_t connection_id);

  // Mark connection as timedout
  static HeartbeatFailedNodeInfo* mark_connection_timed_out(
      std::uint32_t connection_id,
      std::uint64_t last_desc = ConstVariables::MINUS_ONE,
      std::uint64_t timeslice_trigger = ConstVariables::MINUS_ONE);

  // Get the number of active connections
  static std::uint32_t get_active_connection_count();

  // Get the number of timeout connections
  static std::uint32_t get_timeout_connection_count();

  //// Variables
private:
  static HeartbeatManager* heartbeat_manager_;
};
} // namespace tl_libfabric
