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

  //// Variables
private:
  static HeartbeatManager* heartbeat_manager_;
};
} // namespace tl_libfabric
