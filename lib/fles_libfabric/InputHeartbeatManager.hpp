// Copyright 2019 Farouk Salem <salem@zib.de>

#pragma once

#include "HeartbeatManager.hpp"

#include <cassert>
#include <chrono>
#include <log.hpp>
#include <math.h>
#include <set>
#include <string>
#include <vector>

#include <fstream>
#include <iomanip>

namespace tl_libfabric {
/**
 * Singleton Heart Beat manager that DFS uses to detect the failure of a
 * connection
 */
class InputHeartbeatManager : public HeartbeatManager {
public:
  // Initialize the instance and retrieve it
  static InputHeartbeatManager* get_instance(uint32_t index,
                                             uint32_t init_connection_count,
                                             uint64_t init_heartbeat_timeout,
                                             uint32_t timeout_history_size,
                                             uint32_t timeout_factor,
                                             uint32_t inactive_factor,
                                             uint32_t inactive_retry_count,
                                             std::string log_directory,
                                             bool enable_logging);

  // Get singleton instance
  static InputHeartbeatManager* get_instance();

  // Get the first timeslice trigger to next send TSs after it
  uint64_t get_timeslice_blockage_trigger();

  // Remove timeslice trigger of connection after decision arrival
  bool remove_timeslice_blockage_trigger(uint32_t connection_id);

  // Generate log files of the stored data
  // void generate_log_files();

private:
  InputHeartbeatManager(uint32_t index,
                        uint32_t init_connection_count,
                        uint64_t init_heartbeat_timeout,
                        uint32_t timeout_history_size,
                        uint32_t timeout_factor,
                        uint32_t inactive_factor,
                        uint32_t inactive_retry_count,
                        std::string log_directory,
                        bool enable_logging);

  // The singleton instance for this class
  static InputHeartbeatManager* instance_;
};
} // namespace tl_libfabric
