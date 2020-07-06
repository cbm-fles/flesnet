// Copyright 2019 Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
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

protected:
  struct HeartbeatMessageInfo {
    HeartbeatMessage message;
    std::chrono::high_resolution_clock::time_point transmit_time;
    std::chrono::high_resolution_clock::time_point completion_time;
    bool acked = false;
    uint32_t dest_connection;
  };

  HeartbeatManager(uint32_t index,
                   uint32_t init_connection_count,
                   std::string log_directory,
                   bool enable_logging);

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

  // The log directory
  std::string log_directory_;

  bool enable_logging_;
};
} // namespace tl_libfabric
