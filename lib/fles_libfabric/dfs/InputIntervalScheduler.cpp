// Copyright 2018 Farouk Salem <salem@zib.de>

#include "InputIntervalScheduler.hpp"

#include <fstream>
#include <iomanip>

namespace tl_libfabric {
// PUBLIC
InputIntervalScheduler*
InputIntervalScheduler::get_instance(uint32_t scheduler_index,
                                     uint32_t compute_conn_count,
                                     uint32_t interval_length,
                                     std::string log_directory,
                                     bool enable_logging) {
  if (instance_ == nullptr) {
    instance_ = new InputIntervalScheduler(scheduler_index, compute_conn_count,
                                           interval_length, log_directory,
                                           enable_logging);
  }
  return instance_;
}

InputIntervalScheduler* InputIntervalScheduler::get_instance() {
  assert(instance_ != nullptr);
  return instance_;
}

void InputIntervalScheduler::update_compute_connection_count(
    uint32_t compute_count) {
  compute_count_ = compute_count;
}

void InputIntervalScheduler::update_input_begin_time(
    std::chrono::high_resolution_clock::time_point begin_time) {
  begin_time_ = begin_time;
  if (interval_info_.empty()) {
    // create the first interval
    create_new_interval_info(0);
  }
}

bool InputIntervalScheduler::add_proposed_meta_data(
    const IntervalMetaData meta_data) {
  if (proposed_interval_meta_data_.contains(meta_data.interval_index))
    return false;

  proposed_interval_meta_data_.add(meta_data.interval_index,
                                   new IntervalMetaData(meta_data));
  if (true) {
    L_(info) << "[i " << scheduler_index_ << "] "
             << "interval" << meta_data.interval_index << "[TSs "
             << meta_data.start_timeslice << " to " << meta_data.last_timeslice
             << " is proposed and should start after "
             << std::chrono::duration_cast<std::chrono::microseconds>(
                    meta_data.start_time -
                    std::chrono::high_resolution_clock::now())
                    .count()
             << " us & take " << meta_data.interval_duration << " us in "
             << meta_data.round_count << " rounds";
  }
  return true;
}

const IntervalMetaData*
InputIntervalScheduler::get_actual_meta_data(uint64_t interval_index) {
  return actual_interval_meta_data_.contains(interval_index)
             ? actual_interval_meta_data_.get(interval_index)
             : nullptr;
}

uint64_t InputIntervalScheduler::get_last_timeslice_to_send() {
  InputIntervalInfo* current_interval =
      interval_info_.get(interval_info_.get_last_key());
  if (false)
    L_(debug) << "[last to send] last key " << interval_info_.get_last_key()
              << " indx " << current_interval->index << " count_sent_ts "
              << current_interval->count_sent_ts << " count_acked_ts "
              << current_interval->count_acked_ts << " start_ts "
              << current_interval->start_ts << " end_ts "
              << current_interval->end_ts;
  if (current_interval->count_sent_ts == 0 &&
      current_interval->proposed_start_time >
          std::chrono::high_resolution_clock::now())
    return current_interval->index > 0 ? current_interval->start_ts - 1 : 0;
  uint64_t next_round =
      get_interval_expected_round_index(current_interval->index) + 1;
  return std::min(current_interval->start_ts +
                      (next_round * current_interval->num_ts_per_round) - 1,
                  current_interval->end_ts);
}

void InputIntervalScheduler::increament_sent_timeslices(uint64_t timeslice) {
  InputIntervalInfo* current_interval = get_interval_of_timeslice(timeslice);
  assert(current_interval != nullptr);
  if (current_interval->count_sent_ts == 0)
    current_interval->actual_start_time =
        std::chrono::high_resolution_clock::now();
  current_interval->count_sent_ts++;
  if (is_interval_sent_completed(current_interval->index) &&
      is_ack_percentage_reached(current_interval->index) &&
      (current_interval->index == 0 ||
       is_interval_sent_ack_completed(current_interval->index - 1)) &&
      !interval_info_.contains(current_interval->index + 1)) {
    create_new_interval_info(current_interval->index + 1);
  }
}

void InputIntervalScheduler::undo_increament_sent_timeslices(
    std::vector<uint64_t> undo_timeslices) {
  assert(!interval_info_.empty());
  for (uint32_t i = 0; i < undo_timeslices.size(); i++) {
    InputIntervalInfo* interval = get_interval_of_timeslice(undo_timeslices[i]);
    L_(debug) << "[UNDO INTERVAL] BEFORE indx = " << interval->index
              << " count sent " << interval->count_sent_ts << " count acked "
              << interval->count_acked_ts;
    assert(interval->count_sent_ts > 0);
    --interval->count_sent_ts;
    assert(interval->count_sent_ts >= interval->count_acked_ts);
  }
}

void InputIntervalScheduler::increament_acked_timeslices(uint64_t timeslice) {
  InputIntervalInfo* current_interval = get_interval_of_timeslice(timeslice);
  if (current_interval == nullptr)
    return;
  current_interval->count_acked_ts++;
  if (is_ack_percentage_reached(current_interval->index) &&
      is_interval_sent_completed(current_interval->index) &&
      (current_interval->index == 0 ||
       is_interval_sent_ack_completed(current_interval->index - 1)) &&
      !interval_info_.contains(current_interval->index + 1)) {
    create_new_interval_info(current_interval->index + 1);
  }
  if (is_interval_sent_ack_completed(current_interval->index)) {
    create_actual_interval_meta_data(current_interval);
  }
}

int64_t InputIntervalScheduler::get_next_fire_time() {
  uint64_t interval = interval_info_.get_last_key();
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  current_interval->rounds_counter++;

  std::chrono::high_resolution_clock::time_point now =
      std::chrono::high_resolution_clock::now();

  // If  no proposed duration or the proposed finish time is reached, then
  // send as fast as possible.
  if (current_interval->duration_per_ts == 0 ||
      (!is_ack_percentage_reached(interval) &&
       (current_interval->proposed_start_time +
        std::chrono::microseconds(current_interval->proposed_duration)) < now))
    return ConstVariables::ZERO;

  // If the interval is new
  if (current_interval->count_sent_ts == 0) {
    if (current_interval->proposed_start_time <= now)
      return ConstVariables::ZERO;
    return std::chrono::duration_cast<std::chrono::microseconds>(
               current_interval->proposed_start_time - now)
        .count();
  }

  uint64_t expected_sent_ts = get_expected_sent_ts_count(interval);

  int64_t duration = current_interval->duration_per_round +
                     ((current_interval->count_sent_ts - expected_sent_ts) *
                      current_interval->duration_per_ts);
  if (duration < 0)
    duration = 0;

  return duration;
}

uint32_t InputIntervalScheduler::get_compute_connection_count() {
  return compute_count_;
}

uint64_t
InputIntervalScheduler::get_ack_sent_remaining_gap(uint64_t interval_index) {
  if (interval_info_.contains(interval_index)) {
    InputIntervalInfo* current_interval = interval_info_.get(interval_index);
    return (current_interval->end_ts - current_interval->start_ts + 1) *
           (1.0 - minimum_ack_percentage_to_start_new_interval_);
  }
  if (proposed_interval_meta_data_.contains(interval_index)) {
    IntervalMetaData* next_interval =
        proposed_interval_meta_data_.get(interval_index);
    return (next_interval->last_timeslice - next_interval->start_timeslice +
            1) *
           (1.0 - minimum_ack_percentage_to_start_new_interval_);
  }
  return ConstVariables::MINUS_ONE;
}

// PRIVATE
InputIntervalScheduler::InputIntervalScheduler(uint32_t scheduler_index,
                                               uint32_t compute_conn_count,
                                               uint32_t interval_length,
                                               std::string log_directory,
                                               bool enable_logging)
    : scheduler_index_(scheduler_index), compute_count_(compute_conn_count),
      interval_length_(interval_length), log_directory_(log_directory),
      enable_logging_(enable_logging) {
  minimum_ack_percentage_to_start_new_interval_ = 0.95;
}

void InputIntervalScheduler::create_new_interval_info(uint64_t interval_index) {
  InputIntervalInfo* new_interval_info = nullptr;

  if (proposed_interval_meta_data_.contains(
          interval_index)) { // proposed info exist
    IntervalMetaData* proposed_meta_data =
        proposed_interval_meta_data_.get(interval_index);

    // check if there is a gap in ts due to un-received meta-data
    const InputIntervalInfo* prev_interval =
        interval_info_.get(interval_index - 1);

    uint64_t start_timeslice = prev_interval->end_ts + 1,
             last_timeslice = proposed_meta_data->last_timeslice;
    assert(last_timeslice > start_timeslice);
    uint32_t round_count = proposed_meta_data->round_count;
    // TODO
    /*uint32_t ts_count = (last_timeslice - start_timeslice + 1);
    if (ts_count % get_compute_connection_count() != 0){
        round_count = (ts_count / get_compute_connection_count());
        last_timeslice = start_timeslice +
    (round_count*get_compute_connection_count()) - 1;
    }*/

    new_interval_info = new InputIntervalInfo(
        interval_index, round_count, start_timeslice, last_timeslice,
        proposed_meta_data->start_time, proposed_meta_data->interval_duration,
        compute_count_);
  } else {
    if (interval_info_.empty()) { // first interval
      uint32_t round_count = floor(interval_length_ / compute_count_);
      new_interval_info = new InputIntervalInfo(
          interval_index, round_count, 0, (round_count * compute_count_) - 1,
          std::chrono::high_resolution_clock::now(), 0, compute_count_);

    } else { // following last proposed meta-data
      // TODO wait for the proposing!!!
      InputIntervalInfo* prev_interval = interval_info_.get(interval_index - 1);
      new_interval_info = new InputIntervalInfo(
          interval_index, prev_interval->round_count, prev_interval->end_ts + 1,
          prev_interval->end_ts + (prev_interval->round_count * compute_count_),
          prev_interval->proposed_start_time +
              std::chrono::microseconds(prev_interval->proposed_duration),
          prev_interval->proposed_duration, compute_count_);
    }
  }

  if (true) {
    L_(info) << "[i " << scheduler_index_ << "] "
             << "interval" << interval_index << "[TSs "
             << new_interval_info->start_ts << " to "
             << new_interval_info->end_ts << "] should start after "
             << std::chrono::duration_cast<std::chrono::microseconds>(
                    new_interval_info->proposed_start_time -
                    std::chrono::high_resolution_clock::now())
                    .count()
             << " us & take " << new_interval_info->proposed_duration << " us";
  }

  interval_info_.add(interval_index, new_interval_info);
}

void InputIntervalScheduler::create_actual_interval_meta_data(
    InputIntervalInfo* interval_info) {
  if (actual_interval_meta_data_.contains(interval_info->index))
    return;
  interval_info->actual_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() -
          interval_info->actual_start_time)
          .count();

  IntervalMetaData* actual_metadata = new IntervalMetaData(
      interval_info->index, interval_info->round_count, interval_info->start_ts,
      interval_info->end_ts, interval_info->actual_start_time,
      interval_info->actual_duration, get_compute_connection_count());
  if (true) {
    L_(info) << "[i " << scheduler_index_ << "] "
             << "interval" << actual_metadata->interval_index << "[TSs "
             << actual_metadata->start_timeslice << " to "
             << actual_metadata->last_timeslice
             << "] is finished and delayed for "
             << std::chrono::duration_cast<std::chrono::microseconds>(
                    actual_metadata->start_time -
                    interval_info->proposed_start_time)
                    .count()
             << " us & took " << actual_metadata->interval_duration
             << " us [proposed: " << interval_info->proposed_duration << "] in "
             << interval_info->rounds_counter << " rounds";
  }
  actual_interval_meta_data_.add(interval_info->index, actual_metadata);
}

uint64_t InputIntervalScheduler::get_expected_sent_ts_count(uint64_t interval) {
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  if (current_interval->duration_per_ts == 0)
    return (current_interval->end_ts - current_interval->start_ts + 1);
  std::chrono::high_resolution_clock::time_point now =
      std::chrono::high_resolution_clock::now();
  if (now < current_interval->actual_start_time)
    return 0;
  uint64_t max_interval_ts_count =
      current_interval->end_ts - current_interval->start_ts + 1;
  assert(current_interval->duration_per_ts > 0);
  return std::min(max_interval_ts_count,
                  std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::high_resolution_clock::now() -
                      current_interval->actual_start_time)
                          .count() /
                      current_interval->duration_per_ts);
}

uint64_t
InputIntervalScheduler::get_interval_expected_round_index(uint64_t interval) {
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  assert(current_interval->num_ts_per_round > 0);
  return get_expected_sent_ts_count(interval) /
         current_interval->num_ts_per_round;
}

InputIntervalInfo InputIntervalScheduler::get_current_interval_info() {
  return (*interval_info_.get(interval_info_.get_last_key()));
}

InputIntervalInfo*
InputIntervalScheduler::get_interval_of_timeslice(uint64_t timeslice) {
  SizedMap<uint64_t, InputIntervalInfo*>::iterator end_it =
      interval_info_.get_end_iterator();
  do {
    --end_it;
    if (timeslice >= end_it->second->start_ts &&
        timeslice <= end_it->second->end_ts)
      return end_it->second;
  } while (end_it != interval_info_.get_begin_iterator());
  return nullptr;
}

bool InputIntervalScheduler::is_interval_sent_completed(uint64_t interval) {
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  return current_interval->count_sent_ts ==
                 (current_interval->end_ts - current_interval->start_ts + 1)
             ? true
             : false;
}

bool InputIntervalScheduler::is_interval_sent_ack_completed(uint64_t interval) {
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  return is_interval_sent_completed(interval) &&
                 current_interval->count_sent_ts ==
                     current_interval->count_acked_ts
             ? true
             : false;
}

bool InputIntervalScheduler::is_ack_percentage_reached(uint64_t interval) {
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  // TODO change the percentage to be configurable
  return (current_interval->count_acked_ts *
          1.0) / ((current_interval->end_ts - current_interval->start_ts + 1) *
                  1.0) >=
                 minimum_ack_percentage_to_start_new_interval_
             ? true
             : false;
}

std::chrono::high_resolution_clock::time_point
InputIntervalScheduler::get_expected_ts_sent_time(uint64_t interval,
                                                  uint64_t timeslice) {
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  return current_interval->actual_start_time +
         std::chrono::microseconds((timeslice - current_interval->start_ts) *
                                   current_interval->duration_per_ts);
}

std::chrono::high_resolution_clock::time_point
InputIntervalScheduler::get_expected_round_sent_time(uint64_t interval,
                                                     uint64_t round) {
  InputIntervalInfo* current_interval = interval_info_.get(interval);
  return current_interval->actual_start_time +
         std::chrono::microseconds(round *
                                   current_interval->duration_per_round);
}

void InputIntervalScheduler::add_compute_buffer_blockage_duration(
    uint32_t compute_index, uint64_t timeslice, uint64_t duration) {
  InputIntervalInfo* current_interval = get_interval_of_timeslice(timeslice);
  assert(current_interval != nullptr);
  current_interval->sum_compute_blockage_durations_[compute_index] += duration;
}

void InputIntervalScheduler::add_input_buffer_blockage_duration(
    uint32_t compute_index, uint64_t timeslice, uint64_t duration) {
  InputIntervalInfo* current_interval = get_interval_of_timeslice(timeslice);
  assert(current_interval != nullptr);
  current_interval->sum_input_blockage_durations_[compute_index] += duration;
}

void InputIntervalScheduler::generate_log_files() {
  if (!enable_logging_)
    return;

  std::ofstream log_file;
  log_file.open(log_directory_ + "/" + std::to_string(scheduler_index_) +
                ".input.proposed_actual_interval_info.out");

  log_file << std::setw(25) << "Interval" << std::setw(25) << "proposed time"
           << std::setw(25) << "Actual time" << std::setw(25)
           << "Proposed duration" << std::setw(25) << "Actual duration"
           << "\n";

  SizedMap<uint64_t, IntervalMetaData*>::iterator it_actual =
      actual_interval_meta_data_.get_begin_iterator();
  IntervalMetaData* proposed_metadata = nullptr;
  uint64_t proposed_time, actual_time;
  while (it_actual != actual_interval_meta_data_.get_end_iterator()) {
    proposed_metadata = proposed_interval_meta_data_.contains(it_actual->first)
                            ? proposed_interval_meta_data_.get(it_actual->first)
                            : nullptr;
    proposed_time = proposed_metadata == nullptr
                        ? 0
                        : std::chrono::duration_cast<std::chrono::milliseconds>(
                              proposed_metadata->start_time - begin_time_)
                              .count();
    actual_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                      it_actual->second->start_time - begin_time_)
                      .count();
    log_file << std::setw(25) << it_actual->first << std::setw(25)
             << proposed_time << std::setw(25) << actual_time << std::setw(25)
             << (proposed_metadata == nullptr
                     ? 0
                     : proposed_metadata->interval_duration)
             << std::setw(25) << it_actual->second->interval_duration << "\n";

    it_actual++;
  }
  log_file.flush();
  log_file.close();
}

InputIntervalScheduler* InputIntervalScheduler::instance_ = nullptr;

} // namespace tl_libfabric
