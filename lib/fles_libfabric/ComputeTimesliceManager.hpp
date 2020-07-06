// Copyright 2019 Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
#include "SizedMap.hpp"

#include <cassert>
#include <chrono>
#include <log.hpp>
#include <map>
#include <math.h>
#include <set>
#include <string>

#include <fstream>
#include <iomanip>

namespace tl_libfabric {
/**
 * Singleton Timeslice manager that DDScheduler uses to track timeslices
 * completion and timeout
 */
class ComputeTimesliceManager {
public:
  // Initialize the instance and retrieve it
  static ComputeTimesliceManager* get_instance(uint32_t compute_index,
                                               uint32_t input_connection_count,
                                               std::string log_directory,
                                               bool enable_logging);

  // Get singleton instance
  static ComputeTimesliceManager* get_instance();

  // Update input connection count
  void update_input_connection_count(uint32_t input_connection_count);

  // Set the begin time to be used in logging (the trigger_completion flag is
  // used to stop processing timeslices after rescheduling operation until
  // receiving the ACK)
  void log_contribution_arrival(uint32_t connection_id, uint64_t timeslice);

  // Set the begin time to be used in logging
  bool undo_log_contribution_arrival(uint32_t connection_id,
                                     uint64_t timeslice);

  // Get last ordered completed timeslice
  uint64_t get_last_ordered_completed_timeslice();

  // Check timeslices that should time out
  void log_timeout_timeslice();

  // Check whether a timeslice is timed out
  bool is_timeslice_timed_out(uint64_t timeslice);

  // Generate log files of the stored data
  void generate_log_files();

private:
  ComputeTimesliceManager(uint32_t compute_index,
                          uint32_t input_connection_count,
                          std::string log_directory,
                          bool enable_logging);

  // Complete a timeslice and log the completion duration
  void trigger_timeslice_completion(uint64_t timeslice);

  // The first arrival time of each timeslice
  SizedMap<uint64_t, std::chrono::high_resolution_clock::time_point>
      timeslice_first_arrival_time_;

  // Counts the number of received contributions of each timeslice
  SizedMap<uint64_t, std::set<uint32_t>*> timeslice_arrived_count_;

  // The singleton instance for this class
  static ComputeTimesliceManager* instance_;

  // Compute process index
  uint32_t compute_index_;

  // The number of input connections
  uint32_t input_connection_count_;

  // Timeout limit in seconds
  double timeout_;

  // The log directory
  std::string log_directory_;

  bool enable_logging_;

  // Last ordered timeslice which is either completed or timed out
  uint64_t last_ordered_timeslice_ = ConstVariables::MINUS_ONE;

  // LOGGING
  // Time to complete each timeslice
  SizedMap<uint64_t, double> timeslice_completion_duration_;

  // The timed out Timeslices <Timeslice_id, duration since first arrival>
  SizedMap<uint64_t, double> timeslice_timed_out_;
};
} // namespace tl_libfabric
