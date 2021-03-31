// Copyright 2019 Farouk Salem <salem@zib.de>

#include "HeartbeatManager.hpp"

namespace tl_libfabric {

HeartbeatManager::HeartbeatManager(uint32_t index,
                                   uint32_t init_connection_count,
                                   uint64_t init_heartbeat_timeout,
                                   uint32_t timeout_history_size,
                                   uint32_t timeout_factor,
                                   uint32_t inactive_factor,
                                   uint32_t inactive_retry_count,
                                   std::string log_directory,
                                   bool enable_logging)
    : index_(index), connection_count_(init_connection_count),
      init_heartbeat_timeout_(init_heartbeat_timeout),
      timeout_history_size_(timeout_history_size),
      timeout_factor_(timeout_factor), inactive_factor_(inactive_factor),
      inactive_retry_count_(inactive_retry_count),
      log_directory_(log_directory), enable_logging_(enable_logging) {

  assert(inactive_factor_ < timeout_factor_);

  update_latency_log_ = true;
  for (uint32_t i = 0; i < init_connection_count; i++) {
    unacked_sent_messages_.add(i, new std::set<uint64_t>());
    connection_heartbeat_time_.push_back(new ConnectionHeartbeatInfo(
        timeout_history_size * init_heartbeat_timeout));
    connection_heartbeat_time_[i]->latency_history.resize(
        timeout_history_size, init_heartbeat_timeout);
  }
  pending_messages_.resize(init_connection_count);

  L_(debug) << "[HeartbeatManager] init_heartbeat_timeout "
            << init_heartbeat_timeout << " timeout_history_size "
            << timeout_history_size << " timeout_factor " << timeout_factor
            << " inactive_factor " << inactive_factor
            << " inactive_retry_count " << inactive_retry_count;
}

bool HeartbeatManager::check_whether_connection_inactive(
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
      timeout_history_size_;

  if (duration >= (avg_latency * inactive_factor_)) {
    return true;
  }
  return false;
}

bool HeartbeatManager::check_whether_connection_timed_out(
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
      timeout_history_size_;

  if (duration >= (avg_latency * timeout_factor_))
    return true;
  return false;
}

void HeartbeatManager::log_sent_heartbeat_message(uint32_t connection_id,
                                                  HeartbeatMessage message) {
  HeartbeatMessageInfo* message_info = new HeartbeatMessageInfo();
  message_info->message = message;
  message_info->transmit_time = std::chrono::high_resolution_clock::now();
  message_info->dest_connection = connection_id;
  heartbeat_message_log_.add(message.message_id, message_info);
  unacked_sent_messages_.get(connection_id)->insert(message.message_id);
}

uint64_t HeartbeatManager::get_next_heartbeat_message_id() {
  if (heartbeat_message_log_.empty())
    return ConstVariables::ZERO;
  return heartbeat_message_log_.get_last_key() + 1;
}

void HeartbeatManager::ack_message_received(uint64_t messsage_id) {
  if (!heartbeat_message_log_.contains(messsage_id))
    return;
  HeartbeatMessageInfo* message_info = heartbeat_message_log_.get(messsage_id);
  if (message_info->acked) {
    return;
  }
  message_info->acked = true;
  message_info->completion_time = std::chrono::high_resolution_clock::now();

  // remove pending unacked entries
  std::set<uint64_t>::iterator pending =
      unacked_sent_messages_.get(message_info->dest_connection)
          ->find(messsage_id);
  if (pending !=
      unacked_sent_messages_.get(message_info->dest_connection)->end()) {
    unacked_sent_messages_.get(message_info->dest_connection)
        ->erase(
            unacked_sent_messages_.get(message_info->dest_connection)->begin(),
            ++pending);
  }
}

uint32_t HeartbeatManager::count_unacked_messages(uint32_t connection_id) {
  assert(unacked_sent_messages_.size() > connection_id);
  return unacked_sent_messages_.get(connection_id)->size();
}

void HeartbeatManager::add_pending_message(uint32_t connection_id,
                                           HeartbeatMessage* message) {
  assert(connection_id < pending_messages_.size());
  pending_messages_[connection_id].push_back(message);
}

HeartbeatMessage*
HeartbeatManager::get_pending_message(uint32_t connection_id) {
  assert(connection_id < pending_messages_.size());
  if (pending_messages_[connection_id].empty())
    return nullptr;
  HeartbeatMessage* head_message = pending_messages_[connection_id][0];
  pending_messages_[connection_id].erase(
      pending_messages_[connection_id].begin());
  return head_message;
}

void HeartbeatManager::clear_pending_messages(uint32_t connection_id) {
  pending_messages_[connection_id].clear();
}

void HeartbeatManager::log_heartbeat(uint32_t connection_id) {
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
  if (update_latency_log_) {
    log_new_latency(connection_id, time_gap);
  }
}

void HeartbeatManager::log_new_latency(uint32_t connection_id,
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
      (conn_info->next_latency_index + 1) % timeout_history_size_;
}

std::vector<uint32_t> HeartbeatManager::retrieve_new_inactive_connections() {
  std::vector<uint32_t> conns;
  for (uint32_t i = 0; i < connection_heartbeat_time_.size(); i++) {
    if (check_whether_connection_inactive(i) &&
        count_unacked_messages(i) < inactive_retry_count_) {
      if (inactive_connection_.find(i) == inactive_connection_.end()) {
        inactive_connection_.insert(i);
      }
      conns.push_back(i);
    }
  }
  return conns;
}

const std::set<uint32_t> HeartbeatManager::retrieve_timeout_connections() {
  std::set<uint32_t> timed_out_connection_;
  SizedMap<uint32_t, HeartbeatFailedNodeInfo*>::iterator it =
      timeout_node_info_.get_begin_iterator();
  while (it != timeout_node_info_.get_end_iterator()) {
    timed_out_connection_.insert(it->first);
    ++it;
  }
  return timed_out_connection_;
}

int32_t HeartbeatManager::get_new_timeout_connection() {
  std::set<uint32_t>::iterator conn = inactive_connection_.begin();
  while (conn != inactive_connection_.end()) {
    if (check_whether_connection_timed_out(*conn) &&
        count_unacked_messages(*conn) >= inactive_retry_count_ &&
        !timeout_node_info_.contains(*conn)) {
      return *conn;
    }
    ++conn;
  }
  return -1;
}

bool HeartbeatManager::is_connection_inactive(uint32_t connection_id) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (inactive_connection_.find(connection_id) != inactive_connection_.end())
    return true;
  return false;
}

bool HeartbeatManager::is_connection_timed_out(uint32_t connection_id) {
  assert(connection_id < connection_heartbeat_time_.size());
  if (timeout_node_info_.contains(connection_id))
    return true;
  return false;
}

uint32_t HeartbeatManager::get_active_connection_count() {
  assert(timeout_node_info_.size() <= connection_count_);
  return connection_count_ - timeout_node_info_.size();
}

uint32_t HeartbeatManager::get_timeout_connection_count() {
  assert(timeout_node_info_.size() <= connection_count_);
  return timeout_node_info_.size();
}

HeartbeatFailedNodeInfo* HeartbeatManager::mark_connection_timed_out(
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

} // namespace tl_libfabric
