// Copyright 2019 Farouk Salem <salem@zib.de>

#include "InputHeartbeatManager.hpp"

namespace tl_libfabric {

InputHeartbeatManager*
InputHeartbeatManager::get_instance(uint32_t index,
                                    uint32_t init_connection_count,
                                    std::string log_directory,
                                    bool enable_logging) {
  if (instance_ == nullptr) {
    instance_ = new InputHeartbeatManager(index, init_connection_count,
                                          log_directory, enable_logging);
  }
  return instance_;
}

InputHeartbeatManager* InputHeartbeatManager::get_instance() {
  return instance_;
}

InputHeartbeatManager::InputHeartbeatManager(uint32_t index,
                                             uint32_t init_connection_count,
                                             std::string log_directory,
                                             bool enable_logging)
    : HeartbeatManager(
          index, init_connection_count, log_directory, enable_logging) {

  uint64_t timeout = ConstVariables::HEARTBEAT_TIMEOUT;
  for (uint32_t conn = 0; conn < init_connection_count; ++conn) {
    connection_heartbeat_time_.push_back(new ConnectionHeartbeatInfo());
    connection_heartbeat_time_[conn]->latency_history.resize(
        ConstVariables::HEARTBEAT_TIMEOUT_HISTORY_SIZE, timeout);
  }
  assert(ConstVariables::HEARTBEAT_INACTIVE_FACTOR <
         ConstVariables::HEARTBEAT_TIMEOUT_FACTOR);
}

void InputHeartbeatManager::log_heartbeat(uint32_t connection_id) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (timeout_node_info_.contains(connection_id)) {
    L_(warning) << "logging heartbeat of timeout connection " << connection_id;
    return;
  }
  std::chrono::high_resolution_clock::time_point now =
      std::chrono::high_resolution_clock::now();

  ConnectionHeartbeatInfo* conn_info =
      connection_heartbeat_time_[connection_id];
  uint64_t time_gap = std::chrono::duration_cast<std::chrono::microseconds>(
                          now - conn_info->last_received_message)
                          .count();
  conn_info->last_received_message = now;
  // Remove the inactive entry when heartbeat is received
  std::set<uint32_t>::iterator inactive =
      inactive_connection_.find(connection_id);
  if (inactive != inactive_connection_.end()) {
    inactive_connection_.erase(inactive);
  }
  log_new_latency(connection_id, time_gap);
}

void InputHeartbeatManager::log_new_latency(uint32_t connection_id,
                                            uint64_t latency) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (timeout_node_info_.contains(connection_id)) {
    L_(warning) << "logging new latency of timeout connection "
                << connection_id;
    return;
  }
  // L_(info) << "Latency of [" << connection_id << "] is " << latency;
  ConnectionHeartbeatInfo* conn_info =
      connection_heartbeat_time_[connection_id];
  conn_info->sum_latency =
      conn_info->sum_latency -
      conn_info->latency_history[conn_info->next_latency_index] + latency;
  conn_info->latency_history[conn_info->next_latency_index] = latency;
  conn_info->next_latency_index =
      (conn_info->next_latency_index + 1) %
      ConstVariables::HEARTBEAT_TIMEOUT_HISTORY_SIZE;
}

std::vector<uint32_t>
InputHeartbeatManager::retrieve_new_inactive_connections() {
  std::vector<uint32_t> conns;
  for (uint32_t i = 0; i < connection_heartbeat_time_.size(); i++) {
    if (check_whether_connection_inactive(i) &&
        count_unacked_messages(i) <
            ConstVariables::HEARTBEAT_INACTIVE_RETRY_COUNT) {
      if (inactive_connection_.find(i) == inactive_connection_.end()) {
        inactive_connection_.insert(i);
      }
      conns.push_back(i);
    }
  }
  return conns;
}

const std::set<uint32_t> InputHeartbeatManager::retrieve_timeout_connections() {
  std::set<uint32_t> timed_out_connection_;
  SizedMap<uint32_t, HeartbeatFailedNodeInfo*>::iterator it =
      timeout_node_info_.get_begin_iterator();
  while (it != timeout_node_info_.get_end_iterator()) {
    timed_out_connection_.insert(it->first);
    ++it;
  }
  return timed_out_connection_;
}

int32_t InputHeartbeatManager::get_new_timeout_connection() {
  std::set<uint32_t>::iterator conn = inactive_connection_.begin();
  while (conn != inactive_connection_.end()) {
    if (check_whether_connection_timed_out(*conn) &&
        count_unacked_messages(*conn) >=
            ConstVariables::HEARTBEAT_INACTIVE_RETRY_COUNT &&
        !timeout_node_info_.contains(*conn)) {
      return *conn;
    }
    ++conn;
  }
  return -1;
}

bool InputHeartbeatManager::is_connection_inactive(uint32_t connection_id) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (inactive_connection_.find(connection_id) != inactive_connection_.end())
    return true;
  return false;
}

bool InputHeartbeatManager::is_connection_timed_out(uint32_t connection_id) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (timeout_node_info_.contains(connection_id))
    return true;
  return false;
}

bool InputHeartbeatManager::check_whether_connection_inactive(
    uint32_t connection_id) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (inactive_connection_.find(connection_id) != inactive_connection_.end()) {
    return true;
  }
  if (timeout_node_info_.contains(connection_id)) {
    return false;
  }
  double duration =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() -
          connection_heartbeat_time_[connection_id]->last_received_message)
          .count();
  uint64_t avg_latency =
      connection_heartbeat_time_[connection_id]->sum_latency /
      ConstVariables::HEARTBEAT_TIMEOUT_HISTORY_SIZE;

  if (duration >= (avg_latency * ConstVariables::HEARTBEAT_INACTIVE_FACTOR)) {
    return true;
  }
  return false;
}

bool InputHeartbeatManager::check_whether_connection_timed_out(
    uint32_t connection_id) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (timeout_node_info_.contains(connection_id)) {
    return true;
  }
  double duration =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() -
          connection_heartbeat_time_[connection_id]->last_received_message)
          .count();
  uint64_t avg_latency =
      connection_heartbeat_time_[connection_id]->sum_latency /
      ConstVariables::HEARTBEAT_TIMEOUT_HISTORY_SIZE;

  if (duration >= (avg_latency * ConstVariables::HEARTBEAT_TIMEOUT_FACTOR))
    return true;
  return false;
}

HeartbeatFailedNodeInfo* InputHeartbeatManager::mark_connection_timed_out(
    uint32_t connection_id, uint64_t last_desc, uint64_t timeslice_trigger) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (timeout_node_info_.contains(connection_id))
    return timeout_node_info_.get(connection_id);
  if (inactive_connection_.find(connection_id) != inactive_connection_.end()) {
    inactive_connection_.erase(inactive_connection_.find(connection_id));
  }

  HeartbeatFailedNodeInfo* failure_info = new HeartbeatFailedNodeInfo();
  failure_info->index = connection_id;
  failure_info->last_completed_desc = last_desc;
  failure_info->timeslice_trigger = timeslice_trigger;
  timeout_node_info_.add(connection_id, failure_info);
  pending_timed_out_connection_.push_back(
      std::make_pair(timeslice_trigger, connection_id));

  return failure_info;
}

uint64_t InputHeartbeatManager::get_timeslice_blockage_trigger() {
  if (pending_timed_out_connection_.empty()) {
    return ConstVariables::MINUS_ONE;
  }
  uint64_t min = pending_timed_out_connection_.at(0).first;
  for (uint32_t i = 1; i < pending_timed_out_connection_.size(); i++) {
    if (min > pending_timed_out_connection_.at(i).first) {
      min = pending_timed_out_connection_.at(i).first;
    }
  }
  return min;
}

bool InputHeartbeatManager::remove_timeslice_blockage_trigger(
    uint32_t connection_id) {
  if (pending_timed_out_connection_.empty()) {
    return false;
  }

  std::vector<std::pair<uint64_t, uint32_t>>::iterator it =
      pending_timed_out_connection_.begin();
  while (it != pending_timed_out_connection_.end()) {
    if (it->second == connection_id) {
      pending_timed_out_connection_.erase(it);
      return true;
    }
    ++it;
  }
  return false;
}

uint32_t InputHeartbeatManager::get_active_connection_count() {
  assert(timeout_node_info_.size() <= connection_count_);
  return connection_count_ - timeout_node_info_.size();
}

uint32_t InputHeartbeatManager::get_timeout_connection_count() {
  assert(timeout_node_info_.size() < connection_count_);
  return timeout_node_info_.size();
}

InputHeartbeatManager* InputHeartbeatManager::instance_ = nullptr;
} // namespace tl_libfabric
