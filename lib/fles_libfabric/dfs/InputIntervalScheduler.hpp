// Copyright 2018 Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
#include "InputIntervalInfo.hpp"
#include "IntervalMetaData.hpp"
#include "SizedMap.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <log.hpp>
#include <map>
#include <math.h>
#include <set>
#include <string>
#include <vector>

namespace tl_libfabric {
/**
 * Singleton Scheduler for input nodes that could be used in InputChannelSender
 * and InputChannelConnections!
 */
class InputIntervalScheduler {
public:
  // Initialize and get singleton instance
  static InputIntervalScheduler* get_instance(uint32_t scheduler_index,
                                              uint32_t compute_conn_count,
                                              uint32_t interval_length,
                                              std::string log_directory,
                                              bool enable_logging);

  // Get singleton instance
  static InputIntervalScheduler* get_instance();

  // update the compute node count which is needed for the initial interval
  // (#0)
  void update_compute_connection_count(uint32_t);

  // Set the begin time to be used in logging and create the first interval if
  // not there
  void update_input_begin_time(std::chrono::high_resolution_clock::time_point);

  // Receive proposed interval meta-data from InputChannelConnections
  bool add_proposed_meta_data(const IntervalMetaData);

  // Return the actual interval meta-data to InputChannelConnections
  const IntervalMetaData* get_actual_meta_data(uint64_t);

  // Get last timeslice to be sent
  uint64_t get_last_timeslice_to_send();

  // Increase the sent timeslices by one
  void increament_sent_timeslices(uint64_t timeslice);

  // Undo the sent timeslices incremental
  void undo_increament_sent_timeslices(std::vector<uint64_t> undo_timeslices);

  // Increase the acked timeslices by one
  void increament_acked_timeslices(uint64_t);

  // Get the time to start sending more timeslices
  int64_t get_next_fire_time();

  // Get the number of current compute node connections
  uint32_t get_compute_connection_count();

  // Get the gap between ack and sent before start a new interval
  uint64_t get_ack_sent_remaining_gap(uint64_t interval_index);

  // add the blockage duration of a compute node due to full compute buffer
  void add_compute_buffer_blockage_duration(uint32_t compute_index,
                                            uint64_t timeslice,
                                            uint64_t duration);

  // add the blockage duration of a compute node due to full input buffer
  void add_input_buffer_blockage_duration(uint32_t compute_index,
                                          uint64_t timeslice,
                                          uint64_t duration);

  // Get the intervalinfo
  InputIntervalInfo get_current_interval_info();

  // Generate log files of the stored data
  void generate_log_files();

private:
  struct TimesliceInfo {
    std::chrono::high_resolution_clock::time_point expected_time;
    std::chrono::high_resolution_clock::time_point transmit_time;
    uint32_t compute_index;
    uint64_t acked_duration = 0;
  };

  InputIntervalScheduler(uint32_t scheduler_index,
                         uint32_t compute_conn_count,
                         uint32_t interval_length,
                         std::string log_directory,
                         bool enable_logging);

  // The singleton instance for this class
  static InputIntervalScheduler* instance_;

  /// create a new interval with specific index
  void create_new_interval_info(uint64_t);

  // Create an entry in the actual interval meta-data list to be sent to
  // compute schedulers
  void create_actual_interval_meta_data(InputIntervalInfo*);

  // Get the expected number of sent timeslices so far of a particular
  // interval
  uint64_t get_expected_sent_ts_count(uint64_t);

  // Get the expected round index of a particular interval based on round
  // duration
  uint64_t get_interval_expected_round_index(uint64_t);

  // Get the interval info of a particular timeslice
  InputIntervalInfo* get_interval_of_timeslice(uint64_t);

  // Check whether all timeslices of a particular interval are sent out
  bool is_interval_sent_completed(uint64_t);

  // Check whether all timeslices of a particular interval are acked
  bool is_interval_sent_ack_completed(uint64_t);

  // Check whether a specific ack theshold is reached to speedup sending rate
  // to catch others
  bool is_ack_percentage_reached(uint64_t);

  std::chrono::high_resolution_clock::time_point
  get_expected_ts_sent_time(uint64_t interval, uint64_t timeslice);

  std::chrono::high_resolution_clock::time_point
  get_expected_round_sent_time(uint64_t interval, uint64_t round);

  // List of all interval infos
  SizedMap<uint64_t, InputIntervalInfo*> interval_info_;

  // Proposed interval meta-data
  SizedMap<uint64_t, IntervalMetaData*> proposed_interval_meta_data_;

  // Actual interval meta-data
  SizedMap<uint64_t, IntervalMetaData*> actual_interval_meta_data_;

  // Input Scheduler index
  uint32_t scheduler_index_;

  // The number of compute connections
  uint32_t compute_count_;

  // Time at which the InputChannelSender started
  std::chrono::high_resolution_clock::time_point begin_time_;

  // Number of intial timeslices per interval
  uint32_t interval_length_;

  // The Log folder
  std::string log_directory_;

  // Check whether to generate log files
  bool enable_logging_;

  double minimum_ack_percentage_to_start_new_interval_;
};
} // namespace tl_libfabric
