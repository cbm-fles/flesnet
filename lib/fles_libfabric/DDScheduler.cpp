// Copyright 2018 Farouk Salem <salem@zib.de>

#include "DDScheduler.hpp"

// TO BE REMOVED
#include <fstream>
#include <iomanip>

namespace tl_libfabric {
// PUBLIC

DDScheduler* DDScheduler::get_instance(uint32_t scheduler_index,
                                       uint32_t input_scheduler_count,
                                       uint32_t history_size,
                                       uint32_t interval_length,
                                       uint32_t speedup_difference_percentage,
                                       uint32_t speedup_percentage,
                                       uint32_t speedup_interval_count,
                                       std::string log_directory,
                                       bool enable_logging) {
  if (instance_ == nullptr) {
    instance_ = new DDScheduler(
        scheduler_index, input_scheduler_count, history_size, interval_length,
        speedup_difference_percentage, speedup_percentage,
        speedup_interval_count, log_directory, enable_logging);
  }
  return instance_;
}

DDScheduler* DDScheduler::get_instance() { return instance_; }

void DDScheduler::update_clock_offset(
    uint32_t input_index,
    std::chrono::high_resolution_clock::time_point local_time,
    const uint64_t median_latency,
    const uint64_t interval_index) {
  assert(input_scheduler_info_.size() == input_connection_count_);
  if (median_latency == ConstVariables::ZERO) {
    input_scheduler_info_[input_index]->clock_offset =
        std::chrono::duration_cast<std::chrono::microseconds>(begin_time_ -
                                                              local_time)
            .count();
  }
  if (median_latency != ConstVariables::ZERO &&
      interval_index != ConstVariables::ZERO &&
      !input_scheduler_info_[input_index]->interval_info_.contains(
          interval_index)) {
    input_scheduler_info_[input_index]->clock_offset =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - local_time)
            .count() -
        median_latency;
  }
}

void DDScheduler::set_begin_time(
    std::chrono::high_resolution_clock::time_point begin_time) {
  begin_time_ = begin_time;
}

void DDScheduler::add_actual_meta_data(uint32_t input_index,
                                       IntervalMetaData meta_data) {

  if (input_scheduler_info_[input_index]->interval_info_.contains(
          meta_data.interval_index))
    return;

  meta_data.start_time += std::chrono::microseconds(
      input_scheduler_info_[input_index]->clock_offset);
  input_scheduler_info_[input_index]->interval_info_.add(
      meta_data.interval_index, meta_data);

  trigger_complete_interval(meta_data.interval_index);
}

const IntervalMetaData*
DDScheduler::get_proposed_meta_data(uint32_t input_index,
                                    uint64_t interval_index) {
  if (actual_interval_meta_data_.empty())
    return nullptr;

  IntervalMetaData* interval_info;
  if (proposed_interval_meta_data_.contains(interval_index)) {
    interval_info =
        new IntervalMetaData(*proposed_interval_meta_data_.get(interval_index));
  } else {
    interval_info = new IntervalMetaData(
        *calculate_proposed_interval_meta_data(interval_index));
  }
  interval_info->start_time -= std::chrono::microseconds(
      input_scheduler_info_[input_index]->clock_offset);
  return interval_info;
}

uint64_t DDScheduler::get_last_completed_interval() {
  if (actual_interval_meta_data_.empty())
    return ConstVariables::MINUS_ONE;
  return actual_interval_meta_data_.get_last_key();
}

// Generate log files of the stored data
void DDScheduler::generate_log_files() {
  if (!enable_logging_)
    return;

  std::ofstream log_file;
  log_file.open(log_directory_ + "/" + std::to_string(scheduler_index_) +
                ".compute.min_max_interval_info.out");

  log_file << std::setw(25) << "Interval" << std::setw(25) << "Min start"
           << std::setw(25) << "Max start" << std::setw(25) << "Min duration"
           << std::setw(25) << "Max duration" << std::setw(25)
           << "Proposed duration" << std::setw(25) << "Enhanced duration"
           << std::setw(25) << "Speedup Factor" << std::setw(25) << "Rounds"
           << "\n";

  for (SizedMap<uint64_t, IntervalDataLog*>::iterator it =
           interval_info_logger_.get_begin_iterator();
       it != interval_info_logger_.get_end_iterator(); ++it) {
    log_file << std::setw(25) << it->first << std::setw(25)
             << it->second->min_start << std::setw(25) << it->second->max_start
             << std::setw(25) << it->second->min_duration << std::setw(25)
             << it->second->max_duration << std::setw(25)
             << it->second->proposed_duration << std::setw(25)
             << it->second->enhanced_duration << std::setw(25)
             << it->second->speedup_applied << std::setw(25)
             << it->second->rounds_count << "\n";
  }

  log_file.flush();
  log_file.close();
}

// PRIVATE
DDScheduler::DDScheduler(uint32_t scheduler_index,
                         uint32_t input_connection_count,
                         uint32_t history_size,
                         uint32_t interval_length,
                         uint32_t speedup_difference_percentage,
                         uint32_t speedup_percentage,
                         uint32_t speedup_interval_count,
                         std::string log_directory,
                         bool enable_logging)
    : scheduler_index_(scheduler_index),
      input_connection_count_(input_connection_count),
      history_size_(history_size), interval_length_(interval_length),
      speedup_difference_percentage_(speedup_difference_percentage),
      speedup_percentage_(speedup_percentage),
      speedup_interval_count_(speedup_interval_count),
      log_directory_(log_directory), enable_logging_(enable_logging) {

  // TODO check correctness
  compute_node_count_ = input_connection_count;
  for (uint_fast16_t i = 0; i < input_connection_count; i++)
    input_scheduler_info_.push_back(new InputSchedulerData());
}

void DDScheduler::trigger_complete_interval(const uint64_t interval_index) {
  if (actual_interval_meta_data_.contains(interval_index))
    return;

  if (pending_intervals_.contains(interval_index))
    pending_intervals_.update(interval_index,
                              pending_intervals_.get(interval_index) + 1);
  else
    pending_intervals_.add(interval_index, 1);

  // calculate the unified actual meta-data of the whole interval
  if (pending_intervals_.get(interval_index) == input_connection_count_) {
    calculate_interval_info(interval_index);
    pending_intervals_.remove(interval_index);
  }
}

void DDScheduler::calculate_interval_info(uint64_t interval_index) {
  // TODO optimize this part to be only one loop --> O(n) instead of m*O(n)
  std::chrono::high_resolution_clock::time_point average_start_time =
      get_start_time_statistics(interval_index); // get Average
  uint64_t median_interval_duration =
      get_median_interval_duration(interval_index);
  // uint64_t max_round_duration = get_max_round_duration(interval_index);
  // TODO the round count and the start timeslice should be the same from each
  // input scheduler, otherwise, the interval duration should be increased!
  uint32_t average_round_count = get_average_round_count(interval_index);
  uint64_t average_start_timeslice =
      get_average_start_timeslice(interval_index);
  uint64_t average_last_timeslice = get_average_last_timeslice(interval_index);
  compute_node_count_ = get_average_compute_node_count(interval_index);
  actual_interval_meta_data_.add(
      interval_index,
      new IntervalMetaData(interval_index, average_round_count,
                           average_start_timeslice, average_last_timeslice,
                           average_start_time, median_interval_duration,
                           compute_node_count_));

  if (true) {
    L_(info) << "[" << scheduler_index_ << "] interval " << interval_index
             << " [" << average_start_timeslice << ", "
             << average_last_timeslice << "] took " << median_interval_duration
             << " us in " << average_round_count << " rounds"
             << " to " << compute_node_count_ << " compute nodes";
  }

  // LOGGING
  uint64_t min_start_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          get_start_time_statistics(interval_index, 0, 1) - begin_time_)
          .count();
  uint64_t max_start_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          get_start_time_statistics(interval_index, 0, 0) - begin_time_)
          .count();
  if (interval_info_logger_.contains(interval_index)) {
    IntervalDataLog* interval_log = interval_info_logger_.get(interval_index);
    interval_log->min_start = min_start_time;
    interval_log->max_start = max_start_time;
    interval_log->min_duration = get_duration_statistics(interval_index, 0, 1);
    interval_log->max_duration = get_duration_statistics(interval_index, 0, 0);
  } else {
    interval_info_logger_.add(
        interval_index,
        new IntervalDataLog(min_start_time, max_start_time,
                            get_duration_statistics(interval_index, 0, 1),
                            get_duration_statistics(interval_index, 0, 0),
                            average_round_count));
  }
  //
}

const IntervalMetaData*
DDScheduler::calculate_proposed_interval_meta_data(uint64_t interval_index) {
  uint64_t last_interval = actual_interval_meta_data_.get_last_key();
  const IntervalMetaData* last_interval_info =
      actual_interval_meta_data_.get(last_interval);

  uint64_t new_start_timeslice = 0;

  if (!proposed_interval_meta_data_.empty() &&
      proposed_interval_meta_data_.contains(interval_index - 1))
    new_start_timeslice =
        proposed_interval_meta_data_.get(interval_index - 1)->last_timeslice +
        1;
  else
    new_start_timeslice =
        (last_interval_info->last_timeslice + 1) +
        (interval_index - last_interval_info->interval_index - 1) *
            (last_interval_info->last_timeslice -
             last_interval_info->start_timeslice + 1);

  uint32_t active_compute_count =
      compute_node_count_ - compute_node_timeout_count_;

  uint64_t median_interval_duration = get_median_interval_duration_history();
  uint64_t new_interval_duration =
      get_enhanced_interval_duration(interval_index);
  uint32_t round_count = floor(interval_length_ / active_compute_count);
  round_count = round_count == 0 ? 1 : round_count;

  std::chrono::high_resolution_clock::time_point new_start_time =
      last_interval_info->start_time +
      std::chrono::microseconds(median_interval_duration *
                                (interval_index - last_interval));
  // This is commented because it slows down the injection rate specially
  // during the slow start and after the network congection
  /*if (!proposed_interval_meta_data_.empty() &&
  proposed_interval_meta_data_.get_last_key()+1 == interval_index) {
      last_proposed_interval_info =
  proposed_interval_meta_data_.get(proposed_interval_meta_data_.get_last_key());
      std::chrono::high_resolution_clock::time_point last_proposed_end_time =
  last_proposed_interval_info->start_time +
  std::chrono::microseconds(last_proposed_interval_info->interval_duration);
      if (last_proposed_end_time > new_start_time) new_start_time =
  last_proposed_end_time;
  }*/

  IntervalMetaData* new_interval_metadata = new IntervalMetaData(
      interval_index, round_count, new_start_timeslice,
      new_start_timeslice + (round_count * active_compute_count) - 1,
      new_start_time, new_interval_duration, compute_node_count_);
  proposed_interval_meta_data_.add(interval_index, new_interval_metadata);

  // LOGGING
  uint64_t median_duration = get_median_interval_duration_history();
  interval_info_logger_.add(
      interval_index,
      new IntervalDataLog(median_duration, new_interval_duration, round_count,
                          new_interval_duration != median_duration ? 1 : 0));

  return new_interval_metadata;
}

uint64_t DDScheduler::get_enhanced_interval_duration(uint64_t interval_index) {

  if (speedup_interval_index_ != 0 &&
      speedup_interval_index_ + speedup_interval_count_ >=
          interval_index) // in speeding up phase
    return enhanced_interval_duration_;

  if (stabilizing_interval_index_ != 0 &&
      stabilizing_interval_index_ + (speedup_interval_count_ * 2) >=
          interval_index)
    return last_stable_enhanced_interval_duration_;

  uint64_t median_interval_duration = get_median_interval_duration_history();
  double interval_dur_mean_difference =
      get_mean_interval_duration_difference_distory();

  if (speedup_percentage_ > 0 && interval_dur_mean_difference >= 0 &&
      interval_dur_mean_difference / median_interval_duration * 100.0 <=
          speedup_difference_percentage_) {
    if (speedup_interval_index_ != 0 &&
        speedup_interval_index_ + speedup_interval_count_ + 1 ==
            interval_index) {
      last_stable_enhanced_interval_duration_ = enhanced_interval_duration_;
    }
    enhanced_interval_duration_ =
        median_interval_duration -
        (median_interval_duration * speedup_percentage_ / 100.0);
    speedup_interval_index_ = interval_index;
    if (true)
      L_(info) << "[" << scheduler_index_ << "] interval " << interval_index
               << " starting speeding up for " << speedup_interval_count_
               << " intervals[median: " << median_interval_duration
               << ", enhanced: " << enhanced_interval_duration_ << "]";

    return enhanced_interval_duration_;
  }
  if (speedup_interval_index_ != 0 &&
      speedup_interval_index_ + speedup_interval_count_ + 1 == interval_index) {
    stabilizing_interval_index_ = interval_index;
    speedup_interval_index_ = interval_index;
    if (true)
      L_(info) << "[" << scheduler_index_ << "] interval " << interval_index
               << " starting Stabilizing phase for "
               << speedup_interval_count_ * 2
               << " intervals[median: " << median_interval_duration
               << ", proposed: " << last_stable_enhanced_interval_duration_
               << "]";
  }
  return median_interval_duration;
}

std::chrono::high_resolution_clock::time_point
DDScheduler::get_start_time_statistics(uint64_t interval_index,
                                       bool average,
                                       bool min) {
  std::chrono::high_resolution_clock::time_point min_start_time =
                                                     input_scheduler_info_[0]
                                                         ->interval_info_
                                                         .get(interval_index)
                                                         .start_time,
                                                 max_start_time =
                                                     min_start_time,
                                                 tmp_time;
  for (uint32_t i = 1; i < input_connection_count_; i++) {
    tmp_time =
        input_scheduler_info_[i]->interval_info_.get(interval_index).start_time;
    if (min_start_time > tmp_time)
      min_start_time = tmp_time;
    if (max_start_time < tmp_time)
      max_start_time = tmp_time;
  }
  if (average)
    return min_start_time +
           std::chrono::microseconds(
               std::chrono::duration_cast<std::chrono::microseconds>(
                   max_start_time - min_start_time)
                   .count() /
               2);
  return min ? min_start_time : max_start_time;
}

uint64_t DDScheduler::get_duration_statistics(uint64_t interval_index,
                                              bool average,
                                              bool min) {
  uint64_t min_duration = input_scheduler_info_[0]
                              ->interval_info_.get(interval_index)
                              .interval_duration,
           max_duration = min_duration, tmp_duration;
  for (uint32_t i = 1; i < input_connection_count_; i++) {
    tmp_duration = input_scheduler_info_[i]
                       ->interval_info_.get(interval_index)
                       .interval_duration;
    if (min_duration > tmp_duration)
      min_duration = tmp_duration;
    if (max_duration < tmp_duration)
      max_duration = tmp_duration;
  }
  if (average)
    return (min_duration + max_duration) / 2;
  return min ? min_duration : max_duration;
}

uint64_t DDScheduler::get_median_interval_duration(uint64_t interval_index) {
  std::vector<uint64_t> durations(input_connection_count_);
  for (uint32_t i = 0; i < input_connection_count_; i++) {
    durations[i] = input_scheduler_info_[i]
                       ->interval_info_.get(interval_index)
                       .interval_duration;
  }
  std::sort(durations.begin(), durations.end());
  uint32_t mid_index = durations.size() / 2;
  return durations.size() % 2 == 0
             ? (durations[mid_index - 1] + durations[mid_index]) / 2
             : durations[durations.size() / 2];
}

uint64_t DDScheduler::get_median_interval_duration(uint64_t start_interval,
                                                   uint64_t end_interval) {
  std::vector<uint64_t> durations;
  for (uint32_t interval = start_interval; interval <= end_interval;
       interval++) {
    if (!actual_interval_meta_data_.contains(interval))
      break;
    durations.push_back(
        actual_interval_meta_data_.get(interval)->interval_duration);
  }
  assert(!durations.empty());
  std::sort(durations.begin(), durations.end());
  uint32_t mid_index = durations.size() / 2;
  return durations.size() % 2 == 0
             ? (durations[mid_index - 1] + durations[mid_index]) / 2
             : durations[durations.size() / 2];
}

uint32_t DDScheduler::get_average_round_count(uint64_t interval_index) {
  uint32_t round_count = 0;
  for (uint32_t i = 0; i < input_connection_count_; i++)
    round_count += input_scheduler_info_[i]
                       ->interval_info_.get(interval_index)
                       .round_count;
  return round_count / input_connection_count_;
}

uint64_t DDScheduler::get_average_start_timeslice(uint64_t interval_index) {
  uint32_t start_timeslice = 0;
  for (uint32_t i = 0; i < input_connection_count_; i++)
    start_timeslice += input_scheduler_info_[i]
                           ->interval_info_.get(interval_index)
                           .start_timeslice;
  return start_timeslice / input_connection_count_;
}

uint64_t DDScheduler::get_average_last_timeslice(uint64_t interval_index) {
  uint32_t last_timeslice = 0;
  for (uint32_t i = 0; i < input_connection_count_; i++)
    last_timeslice += input_scheduler_info_[i]
                          ->interval_info_.get(interval_index)
                          .last_timeslice;
  return last_timeslice / input_connection_count_;
}

uint32_t DDScheduler::get_average_compute_node_count(uint64_t interval_index) {
  uint32_t node_count = 0;
  for (uint32_t i = 0; i < input_connection_count_; i++)
    node_count += input_scheduler_info_[i]
                      ->interval_info_.get(interval_index)
                      .compute_node_count;
  return node_count / input_connection_count_;
}

uint64_t DDScheduler::get_max_round_duration_history() {
  if (actual_interval_meta_data_.empty())
    return 0;

  uint32_t required_size = history_size_, count = 0;
  uint64_t max_round_duration = 0;

  if (actual_interval_meta_data_.size() < required_size)
    required_size = actual_interval_meta_data_.size();

  SizedMap<uint64_t, IntervalMetaData*>::iterator it =
      actual_interval_meta_data_.get_end_iterator();
  do {
    --it;
    ++count;
    IntervalMetaData* meta_data = it->second;
    uint64_t average_round_duration =
        meta_data->interval_duration / meta_data->round_count;
    if (max_round_duration < average_round_duration)
      max_round_duration = average_round_duration;
  } while (it != actual_interval_meta_data_.get_begin_iterator() &&
           count < required_size);

  return max_round_duration;
}

double DDScheduler::get_mean_round_duration_difference_distory() {
  uint64_t last_completed_interval = actual_interval_meta_data_.get_last_key();

  // +2 because there is no proposed duration for the first two intervals
  if (last_completed_interval < history_size_ + 2)
    return 0;

  double mean = 0;
  uint64_t proposed_round_dur, actual_round_dur;
  const IntervalMetaData *proposed_interval, *actual_interval;
  for (uint32_t i = 0; i < history_size_; i++) {
    proposed_interval =
        proposed_interval_meta_data_.get(last_completed_interval - i);
    proposed_round_dur =
        proposed_interval->interval_duration / proposed_interval->round_count;

    actual_interval =
        actual_interval_meta_data_.get(last_completed_interval - i);
    actual_round_dur =
        actual_interval->interval_duration / actual_interval->round_count;

    mean += actual_round_dur - proposed_round_dur;
  }
  mean /= history_size_;

  return mean < 0 ? 0 : mean;
}

double DDScheduler::get_mean_interval_duration_difference_distory() {
  uint64_t last_completed_interval = actual_interval_meta_data_.get_last_key();

  // +2 because there is no proposed duration for the first two intervals
  if (last_completed_interval < history_size_ + 2)
    return 0;

  double mean = 0;
  int64_t proposed_interval_dur, actual_interval_dur;
  for (uint32_t i = 0; i < history_size_; i++) {
    proposed_interval_dur =
        proposed_interval_meta_data_.get(last_completed_interval - i)
            ->interval_duration;

    actual_interval_dur =
        actual_interval_meta_data_.get(last_completed_interval - i)
            ->interval_duration;

    mean += actual_interval_dur - proposed_interval_dur;
  }
  mean /= history_size_;

  // return any small fraction because negative values mean that actual
  // duration was less than proposed one
  return mean <= 0 ? 0.01 : mean;
}

uint64_t DDScheduler::get_median_interval_duration_history() {
  if (actual_interval_meta_data_.empty())
    return 0;

  uint32_t required_size = history_size_;
  std::vector<uint64_t> durations;

  if (actual_interval_meta_data_.size() < required_size)
    required_size = actual_interval_meta_data_.size();

  SizedMap<uint64_t, IntervalMetaData*>::iterator it =
      actual_interval_meta_data_.get_end_iterator();
  do {
    --it;
    durations.push_back(it->second->interval_duration);
  } while (it != actual_interval_meta_data_.get_begin_iterator() &&
           durations.size() < required_size);

  std::sort(durations.begin(), durations.end());

  return durations.size() % 2 == 0 ? (durations[durations.size() / 2] +
                                      durations[(durations.size() / 2) - 1]) /
                                         2
                                   : durations[durations.size() / 2];
}

void DDScheduler::update_compute_node_timeout_count(uint32_t timeout_count) {
  assert(timeout_count >= compute_node_timeout_count_);
  if (compute_node_timeout_count_ != timeout_count) {
    compute_node_timeout_count_ = timeout_count;
    enhanced_interval_duration_ = get_median_interval_duration_history();
    speedup_interval_index_ = proposed_interval_meta_data_.get_last_key() +
                              1; // start speeding up from the next timeslice
  }
}

DDScheduler* DDScheduler::instance_ = nullptr;
} // namespace tl_libfabric
