// Copyright 2019 Farouk Salem <salem@zib.de>

#include "InputHeartbeatManager.hpp"

namespace tl_libfabric {

InputHeartbeatManager*
InputHeartbeatManager::get_instance(uint32_t index,
                                    uint32_t init_connection_count,
                                    uint64_t init_heartbeat_timeout,
                                    uint32_t timeout_history_size,
                                    uint32_t timeout_factor,
                                    uint32_t inactive_factor,
                                    uint32_t inactive_retry_count,
                                    std::string log_directory,
                                    bool enable_logging) {
  if (instance_ == nullptr) {
    instance_ = new InputHeartbeatManager(
        index, init_connection_count, init_heartbeat_timeout,
        timeout_history_size, timeout_factor, inactive_factor,
        inactive_retry_count, log_directory, enable_logging);
  }
  return instance_;
}

InputHeartbeatManager* InputHeartbeatManager::get_instance() {
  return instance_;
}

InputHeartbeatManager::InputHeartbeatManager(uint32_t index,
                                             uint32_t init_connection_count,
                                             uint64_t init_heartbeat_timeout,
                                             uint32_t timeout_history_size,
                                             uint32_t timeout_factor,
                                             uint32_t inactive_factor,
                                             uint32_t inactive_retry_count,
                                             std::string log_directory,
                                             bool enable_logging)
    : HeartbeatManager(index,
                       init_connection_count,
                       init_heartbeat_timeout,
                       timeout_history_size,
                       timeout_factor,
                       inactive_factor,
                       inactive_retry_count,
                       log_directory,
                       enable_logging) {}

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

InputHeartbeatManager* InputHeartbeatManager::instance_ = nullptr;
} // namespace tl_libfabric
