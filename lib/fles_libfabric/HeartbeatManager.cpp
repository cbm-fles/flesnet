// Copyright 2019 Farouk Salem <salem@zib.de>

#include "HeartbeatManager.hpp"

namespace tl_libfabric {

HeartbeatManager::HeartbeatManager(uint32_t index,
                                   uint32_t init_connection_count,
                                   std::string log_directory,
                                   bool enable_logging)
    : index_(index), connection_count_(init_connection_count),
      log_directory_(log_directory), enable_logging_(enable_logging) {
  for (uint32_t i = 0; i < init_connection_count; i++)
    unacked_sent_messages_.add(i, new std::set<uint64_t>());
  pending_messages_.resize(init_connection_count);
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
} // namespace tl_libfabric
