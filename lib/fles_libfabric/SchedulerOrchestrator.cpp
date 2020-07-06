// Copyright 2020 Farouk Salem <salem@zib.de>

#include "SchedulerOrchestrator.hpp"

namespace tl_libfabric {
//// Common Methods

void SchedulerOrchestrator::initialize(HeartbeatManager* heartbeat_manager) {
  heartbeat_manager_ = heartbeat_manager;
}

//// HeartbeatManager

void SchedulerOrchestrator::log_sent_heartbeat_message(
    uint32_t connection_id, HeartbeatMessage message) {
  heartbeat_manager_->log_sent_heartbeat_message(connection_id, message);
}

uint64_t SchedulerOrchestrator::get_next_heartbeat_message_id() {
  return heartbeat_manager_->get_next_heartbeat_message_id();
}

void SchedulerOrchestrator::acknowledge_heartbeat_message(uint64_t message_id) {
  heartbeat_manager_->ack_message_received(message_id);
}

void SchedulerOrchestrator::add_pending_heartbeat_message(
    uint32_t connection_id, HeartbeatMessage* message) {
  heartbeat_manager_->add_pending_message(connection_id, message);
}

HeartbeatMessage*
SchedulerOrchestrator::get_pending_heartbeat_message(uint32_t connection_id) {
  return heartbeat_manager_->get_pending_message(connection_id);
}

void SchedulerOrchestrator::clear_pending_messages(uint32_t connection_id) {
  heartbeat_manager_->clear_pending_messages(connection_id);
}

//// Variables

HeartbeatManager* SchedulerOrchestrator::heartbeat_manager_ = nullptr;
} // namespace tl_libfabric
