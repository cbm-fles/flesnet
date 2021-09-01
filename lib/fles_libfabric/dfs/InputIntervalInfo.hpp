// Copyright 2017 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include "SizedMap.hpp"
#include <chrono>
#include <utility>
#include <vector>

#pragma pack(1)

namespace tl_libfabric {
struct InputIntervalInfo {
  uint64_t index;
  uint32_t round_count;
  uint64_t start_ts;
  uint64_t end_ts;
  std::chrono::high_resolution_clock::time_point proposed_start_time;
  uint64_t proposed_duration;
  std::chrono::high_resolution_clock::time_point actual_start_time;
  uint64_t actual_duration = ConstVariables::ZERO;

  uint64_t count_sent_ts = ConstVariables::ZERO;

  uint64_t count_acked_ts = ConstVariables::ZERO;

  uint64_t rounds_counter = ConstVariables::ZERO;

  InputIntervalInfo(
      const uint64_t interval_index,
      const uint32_t rounds,
      const uint64_t start_ts,
      const uint64_t
          end_ts, // @suppress("Class members should be properly initialized")
      const std::chrono::high_resolution_clock::time_point start_time,
      const uint64_t duration,
      const uint32_t compute_count)
      : index(interval_index), round_count(rounds), start_ts(start_ts),
        end_ts(end_ts), proposed_start_time(start_time),
        proposed_duration(duration) {
    actual_start_time = proposed_start_time;
    duration_per_ts = proposed_duration / (end_ts - start_ts + 1);
    duration_per_round = proposed_duration / round_count;
    num_ts_per_round = (end_ts - start_ts + 1) / round_count;
    sum_compute_blockage_durations_.resize(compute_count, 0);
    sum_input_blockage_durations_.resize(compute_count, 0);
  }

  uint64_t duration_per_ts = ConstVariables::ZERO;
  uint64_t duration_per_round = ConstVariables::ZERO;
  uint64_t num_ts_per_round = ConstVariables::ZERO;

  std::vector<uint64_t> sum_compute_blockage_durations_;

  std::vector<uint64_t> sum_input_blockage_durations_;
};
} // namespace tl_libfabric

#pragma pack()
