// Copyright 2018 Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
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
 * Singleton Distributed Deterministic Scheduler for compute nodes that could be
 * used in TimesliceScheduler and ComputeNodeConnections!
 */
class DDScheduler {
public:
  // Initialize the instance and retrieve it
  static DDScheduler* get_instance(uint32_t scheduler_index,
                                   uint32_t input_connection_count,
                                   uint32_t history_size,
                                   uint32_t interval_duration,
                                   uint32_t speedup_difference_percentage,
                                   uint32_t speedup_percentage,
                                   uint32_t speedup_interval_count,
                                   std::string log_directory,
                                   bool enable_logging);

  // Get singleton instance
  static DDScheduler* get_instance();

  // Set the input nodes count
  void
  update_clock_offset(uint32_t input_index,
                      std::chrono::high_resolution_clock::time_point MPI_time,
                      const uint64_t median_latency = ConstVariables::ZERO,
                      const uint64_t interval_index = ConstVariables::ZERO);

  // Set the begin time to be used in logging
  void
  set_begin_time(std::chrono::high_resolution_clock::time_point begin_time);

  // Receive actual interval meta-data from ComputeNodeConnection
  void add_actual_meta_data(const uint32_t input_index,
                            IntervalMetaData meta_data);

  // Return the proposed interval meta-data to ComputeNodeConnection
  const IntervalMetaData* get_proposed_meta_data(uint32_t input_index,
                                                 uint64_t interval_index);

  // Check what is the last completed interval
  uint64_t get_last_completed_interval();

  // Update the timeout connection count
  void update_compute_node_timeout_count(uint32_t timeout_count);

  // Generate log files of the stored data
  void generate_log_files();

private:
  struct InputSchedulerData {
    // uint32_t index_;
    // std::chrono::high_resolution_clock::time_point MPI_Barrier_time;
    int64_t clock_offset = 0;
    /// <interval index, <actual_start_time,duration>>. Duration is the
    /// spent time from sending the contribution till getting the
    /// acknowledgement
    SizedMap<uint64_t, IntervalMetaData> interval_info_;
  };

  // LOGGING
  struct IntervalDataLog {
    uint64_t min_start;
    uint64_t max_start;
    uint64_t min_duration;
    uint64_t max_duration;
    uint64_t proposed_duration;
    uint64_t enhanced_duration;
    uint64_t rounds_count;
    bool speedup_applied;
    // Constructor used when proposing an interval
    IntervalDataLog(uint64_t proposed_duration,
                    uint64_t enhanced_duration,
                    uint64_t rounds_count,
                    bool speedup_applied)
        : min_start(0), max_start(0), min_duration(0), max_duration(0),
          proposed_duration(proposed_duration),
          enhanced_duration(enhanced_duration), rounds_count(rounds_count),
          speedup_applied(speedup_applied) {}

    // Constructor used when an entry is created after calculating the
    // actual interval info
    IntervalDataLog(uint64_t min_start,
                    uint64_t max_start,
                    uint64_t min_duration,
                    uint64_t max_duration,
                    uint64_t rounds_count)
        : min_start(min_start), max_start(max_start),
          min_duration(min_duration), max_duration(max_duration),
          proposed_duration(0), enhanced_duration(0),
          rounds_count(rounds_count), speedup_applied(0) {}
  };
  ///

  DDScheduler(uint32_t scheduler_index,
              uint32_t input_connection_count,
              uint32_t history_size,
              uint32_t interval_duration,
              uint32_t speedup_difference_percentage,
              uint32_t speedup_percentage,
              uint32_t speedup_interval_count,
              std::string log_directory,
              bool enable_logging);

  // Trigger when all the actual meta-data have been received to calculate the
  // statistics
  void trigger_complete_interval(const uint64_t interval_index);

  // Calculate the statistics of an interval aftere receiving all the required
  // actual meta-data
  void calculate_interval_info(uint64_t interval_index);

  // Calculate the proposed meta-data of a new/requested interval
  const IntervalMetaData*
  calculate_proposed_interval_meta_data(uint64_t interval_index);

  // Minimize the enhanced interval duration if the variance is low
  uint64_t get_enhanced_interval_duration(uint64_t interval_index);

  // Get statistics about start time of an interval average, minimal, or
  // maximal
  std::chrono::high_resolution_clock::time_point get_start_time_statistics(
      uint64_t interval_index, bool average = true, bool min = false);

  // Get statistics about duration of an interval average, minimal, or maximal
  uint64_t get_duration_statistics(uint64_t interval_index,
                                   bool average = true,
                                   bool min = false);

  // Get the median actual interval duration
  uint64_t get_median_interval_duration(uint64_t interval_index);

  // Get the median actual interval duration of a range
  uint64_t get_median_interval_duration(uint64_t start_interval,
                                        uint64_t end_interval);

  // Get average round count of an interval
  uint32_t get_average_round_count(uint64_t interval_index);

  // Get average start timeslice of an interval
  uint64_t get_average_start_timeslice(uint64_t interval_index);

  // Get average last timeslice of an interval
  uint64_t get_average_last_timeslice(uint64_t interval_index);

  // Get average last timeslice of an interval
  uint32_t get_average_compute_node_count(uint64_t interval_index);

  // Get maximum round duration from the history
  uint64_t get_max_round_duration_history();

  // Get mean difference between different round durations
  double get_mean_round_duration_difference_distory();

  // Get mean difference between different interval durations
  double get_mean_interval_duration_difference_distory();

  // Get the median duration of last set of durations
  uint64_t get_median_interval_duration_history();

  // Proposed interval meta-data
  SizedMap<uint64_t, IntervalMetaData*> proposed_interval_meta_data_;

  // Unified actual interval meta-data
  SizedMap<uint64_t, IntervalMetaData*> actual_interval_meta_data_;

  // Actual interval meta-data
  std::vector<InputSchedulerData*> input_scheduler_info_;

  // Trigger the completed collected intervals to calculate the statistics
  SizedMap<uint64_t, uint32_t> pending_intervals_;

  // The singleton instance for this class
  static DDScheduler* instance_;

  // Input Scheduler index
  uint32_t scheduler_index_;

  // The number of input connections
  uint32_t input_connection_count_;

  // The MPI time of TimesliceBuilder start
  std::chrono::high_resolution_clock::time_point begin_time_;

  // The history size
  uint32_t history_size_;

  // The initial timeslices of each interval
  uint32_t interval_length_;

  // The max variance percentage between proposed and actual duration to
  // speedup
  uint32_t speedup_difference_percentage_;

  // The speedup percentage
  uint32_t speedup_percentage_;

  // The number of intervals to keep speeding up
  uint32_t speedup_interval_count_;

  // The interval number when speeding up is started
  uint64_t speedup_interval_index_ = 0;

  // The enhanced interval duration
  uint64_t enhanced_interval_duration_ = 0;

  // The last stable enhanced interval duration
  uint64_t last_stable_enhanced_interval_duration_ = 0;

  // The interval number when stabilizing is started
  uint64_t stabilizing_interval_index_ = 0;

  // The total count of active & timeout compute nodes
  uint32_t compute_node_count_;

  // The total count of timeout compute nodes
  uint32_t compute_node_timeout_count_ = 0;

  // The log directory
  std::string log_directory_;

  bool enable_logging_;
  // LOGGING
  SizedMap<uint64_t, IntervalDataLog*> interval_info_logger_;
};
} // namespace tl_libfabric
