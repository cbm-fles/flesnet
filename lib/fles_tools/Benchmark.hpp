// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/// %Benchmark class.
class Benchmark {
public:
  Benchmark();
  void run();

  enum class Algorithm {
    Boost_C,
    Boost_I,
#if defined(__ARM_FEATURE_CRC32) || defined(__x86_64)
    Intrinsic32,
#endif
#if defined(__x86_64)
    Intrinsic64,
#endif
    CrcUtil_C,
    CrcUtil_I
  };
  uint32_t compute_crc32(Algorithm algorithm);
  void run_single(Algorithm algorithm);

  const size_t size_ = 1048576;
  const size_t cycles_ = 500;

private:
  std::vector<uint8_t> random_data_;
};
