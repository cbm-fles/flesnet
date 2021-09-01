// Copyright 2020 Farouk Salem <salem@zib.de>

#include "SchedulerOrchestrator.hpp"
#include "HeartbeatFailedNodeInfo.hpp"

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

HeartbeatFailedNodeInfo* SchedulerOrchestrator::mark_connection_timed_out(
    uint32_t connection_id, uint64_t last_desc, uint64_t timeslice_trigger) {
  clear_pending_messages(connection_id);
  return heartbeat_manager_->mark_connection_timed_out(connection_id, last_desc,
                                                       timeslice_trigger);
}

void SchedulerOrchestrator::log_heartbeat(uint32_t connection_id) {
  heartbeat_manager_->log_heartbeat(connection_id);
}

std::vector<uint32_t>
SchedulerOrchestrator::retrieve_new_inactive_connections() {
  return heartbeat_manager_->retrieve_new_inactive_connections();
}

// Retrieve a list of timeout connections
const std::set<uint32_t> SchedulerOrchestrator::retrieve_timeout_connections() {
  return heartbeat_manager_->retrieve_timeout_connections();
}

int32_t SchedulerOrchestrator::get_new_timeout_connection() {
  return heartbeat_manager_->get_new_timeout_connection();
}

bool SchedulerOrchestrator::is_connection_timed_out(uint32_t connection_id) {
  return heartbeat_manager_->is_connection_timed_out(connection_id);
}

uint32_t SchedulerOrchestrator::get_active_connection_count() {
  return heartbeat_manager_->get_active_connection_count();
}

uint32_t SchedulerOrchestrator::get_timeout_connection_count() {
  return heartbeat_manager_->get_timeout_connection_count();
}
//// Variables

HeartbeatManager* SchedulerOrchestrator::heartbeat_manager_ = nullptr;
} // namespace tl_libfabric
