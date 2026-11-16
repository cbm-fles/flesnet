// Copyright 2012-2014 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Microslice.hpp"
#include "MicrosliceSource.hpp"
#include <cstdint>
#include <random>

/// Simple software pattern generator.
/** Produces an endless stream of microslices containing the FLES basic ramp
    pattern. The microslice start times begin at `start_idx` and advance by
    `idx_step`, so that by default the microslices are numbered. */
class FlesnetPatternGenerator : public fles::MicrosliceSource {
public:
  /// The FlesnetPatternGenerator constructor.
  FlesnetPatternGenerator(uint64_t input_index,
                          uint32_t typical_content_size,
                          bool randomize_sizes = false,
                          int64_t start_idx = 0,
                          int64_t idx_step = 1);

  [[nodiscard]] bool eos() const override { return false; }

private:
  fles::Microslice* do_get() override;

  /// The input index, used to distinguish the patterns of different inputs
  uint64_t input_index_;

  uint32_t typical_content_size_;
  bool randomize_sizes_;

  /// A pseudo-random number generator.
  std::default_random_engine random_generator_;

  /// Distribution to use in determining data content sizes.
  std::poisson_distribution<unsigned int> random_distribution_;

  /// Start time of the next microslice
  uint64_t idx_;

  /// Increment of the start time per microslice
  uint64_t idx_step_;

  /// Number of generated microslices
  uint64_t microslice_count_ = 0;

  /// Number of generated content bytes
  uint64_t content_bytes_count_ = 0;
};
