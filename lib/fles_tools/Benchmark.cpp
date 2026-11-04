// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "Benchmark.hpp"
#include "interface.h" // crcutil_interface
#include "log_short.hpp"
#include <algorithm>   // std::generate_n
#include <boost/crc.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <random>
#include <string_view>

#if defined(__x86_64)
#include <smmintrin.h>
#elif defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#endif

Benchmark::Benchmark() {
  random_data_.reserve(size_);

  std::mt19937 engine;
  std::uniform_int_distribution<uint8_t> distribution;
  auto generator = [&] { return distribution(engine); };

  std::generate_n(std::back_inserter(random_data_), size_, generator);
}

// IEEE is by far and away the most common CRC-32 polynomial.
// Used by ethernet (IEEE 802.3), v.42, fddi, gzip, zip, png, ...
// IEEE = 0xedb88320

// Castagnoli's polynomial, used in iSCSI.
// Has better error detection characteristics than IEEE.
// http://dx.doi.org/10.1109/26.231911
// Castagnoli = 0x82f63b78

// Koopman's polynomial.
// Also has better error detection characteristics than IEEE.
// http://dx.doi.org/10.1109/DSN.2002.1028931
// Koopman = 0xeb31d82e

uint32_t Benchmark::compute_crc32(Algorithm algorithm) {
  uint32_t crc = 0;

  switch (algorithm) {

  case Algorithm::Boost_C: {
    // Castagnoli
    boost::crc_optimal<32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true, true>
        crc_32;

    for (size_t i = 0; i < cycles_; ++i) {
      crc_32.process_bytes(random_data_.data(), random_data_.size());
    }
    crc = crc_32();
    break;
  }

  case Algorithm::Boost_I: {
    // IEEE
    boost::crc_32_type crc_32;

    for (size_t i = 0; i < cycles_; ++i) {
      crc_32.process_bytes(random_data_.data(), random_data_.size());
    }
    crc = crc_32();
    break;
  }

  case Algorithm::Intrinsic32: {
    // Castagnoli
    crc ^= 0xFFFFFFFF;
    for (size_t i = 0; i < cycles_; ++i) {
      auto* p = reinterpret_cast<uint32_t*>(random_data_.data());
      const uint32_t* const end = p + random_data_.size() / sizeof(uint32_t);
      while (p < end) {
#if defined(__x86_64)
        crc = _mm_crc32_u32(crc, *p++);
#elif defined(__ARM_FEATURE_CRC32)
        crc = __crc32w(crc, *p++);
#endif
      }
    }
    crc ^= 0xFFFFFFFF;
    break;
  }

#if defined(__x86_64)
  case Algorithm::Intrinsic64: {
    // Castagnoli
    uint64_t crc64 = UINT64_C(0xFFFFFFFF);
    for (size_t i = 0; i < cycles_; ++i) {
      auto* p = reinterpret_cast<uint64_t*>(random_data_.data());
      const uint64_t* const end = p + random_data_.size() / sizeof(uint64_t);
      while (p < end) {
        crc64 = _mm_crc32_u64(crc64, *p++);
      }
    }
    crc = static_cast<uint32_t>(crc64 & 0xffffffffU);
    crc ^= 0xFFFFFFFF;
    break;
  }
#endif

  case Algorithm::CrcUtil_C: {
    // Castagnoli
    crcutil_interface::CRC* crc_32 = crcutil_interface::CRC::Create(
        0x82f63b78, 0, 32, true, 0, 0, 0,
        crcutil_interface::CRC::IsSSE42Available(), nullptr);
    crcutil_interface::UINT64 crc64 = 0;
    for (size_t i = 0; i < cycles_; ++i) {
      crc_32->Compute(random_data_.data(), random_data_.size(), &crc64);
    }
    crc = static_cast<uint32_t>(crc64);
    crc_32->Delete();
    break;
  }

  case Algorithm::CrcUtil_I: {
    // IEEE
    crcutil_interface::CRC* crc_32 = crcutil_interface::CRC::Create(
        0xedb88320, 0, 32, true, 0, 0, 0,
        crcutil_interface::CRC::IsSSE42Available(), nullptr);
    crcutil_interface::UINT64 crc64 = 0;
    for (size_t i = 0; i < cycles_; ++i) {
      crc_32->Compute(random_data_.data(), random_data_.size(), &crc64);
    }
    crc = static_cast<uint32_t>(crc64);
    crc_32->Delete();
    break;
  }
  }

  return crc;
}

void Benchmark::run() {
  run_single(Algorithm::Boost_C, "Boost (Castagnoli)");
  run_single(Algorithm::Boost_I, "Boost (IEEE)");
  run_single(Algorithm::Intrinsic32, "Intrinsic32 (Castagnoli)");
#if defined(__x86_64)
  run_single(Algorithm::Intrinsic64, "Intrinsic64 (Castagnoli)");
#endif
  run_single(Algorithm::CrcUtil_C, "CrcUtil (Castagnoli)");
  run_single(Algorithm::CrcUtil_I, "CrcUtil (IEEE)");
}

void Benchmark::run_single(Algorithm algorithm, std::string_view name) {
  const size_t bytes_ = size_ * cycles_;

  auto start = std::chrono::system_clock::now();
  uint32_t crc32 = compute_crc32(algorithm);
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now() - start);
  // bytes per microsecond
  const double rate =
      static_cast<double>(bytes_) / static_cast<double>(duration.count());
  STATUS("CRC32 benchmark {}: crc32={:x}, {:.1f} MB/s",
         logging::field("algorithm", std::string(name)), crc32, rate,
         logging::field("rate_mb_s", rate));
}
