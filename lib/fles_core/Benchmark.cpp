// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "Benchmark.hpp"
#include "interface.h" // crcutil_interface
#include <boost/crc.hpp>
#include <random>
#include <iostream>
#include <chrono>
#include <functional> // std::bind
#include <algorithm>  // std::generate_n
#include <smmintrin.h>

Benchmark::Benchmark()
{
    random_data_.reserve(size_);

    std::mt19937 engine;
    std::uniform_int_distribution<uint8_t> distribution;
    auto generator = std::bind(distribution, engine);

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

uint32_t Benchmark::compute_crc32(Algorithm algorithm)
{
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
            uint32_t* p = reinterpret_cast<uint32_t*>(random_data_.data());
            const uint32_t* const end =
                p + random_data_.size() / sizeof(uint32_t);
            while (p < end) {
                crc = _mm_crc32_u32(crc, *p++);
            }
        }
        crc ^= 0xFFFFFFFF;
        break;
    }

    case Algorithm::Intrinsic64: {
        // Castagnoli
        uint64_t crc64 = UINT64_C(0xFFFFFFFF);
        for (size_t i = 0; i < cycles_; ++i) {
            uint64_t* p = reinterpret_cast<uint64_t*>(random_data_.data());
            const uint64_t* const end =
                p + random_data_.size() / sizeof(uint64_t);
            while (p < end) {
                crc64 = _mm_crc32_u64(crc64, *p++);
            }
        }
        crc = static_cast<uint32_t>(crc64 & 0xffffffffU);
        crc ^= 0xFFFFFFFF;
        break;
    }

    case Algorithm::CrcUtil_C: {
        // Castagnoli
        crcutil_interface::CRC* crc_32 = crcutil_interface::CRC::Create(
            0x82f63b78, 0, 32, true, 0, 0, 0,
            crcutil_interface::CRC::IsSSE42Available(), NULL);
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
            crcutil_interface::CRC::IsSSE42Available(), NULL);
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

void Benchmark::run()
{
    std::cout << "CRC32 Benchmark: Boost (Castagnoli)" << std::endl;
    run_single(Algorithm::Boost_C);
    std::cout << "CRC32 Benchmark: Boost (IEEE)" << std::endl;
    run_single(Algorithm::Boost_I);
    std::cout << "CRC32 Benchmark: Intrinsic32 (Castagnoli)" << std::endl;
    run_single(Algorithm::Intrinsic32);
    std::cout << "CRC32 Benchmark: Intrinsic64 (Castagnoli)" << std::endl;
    run_single(Algorithm::Intrinsic64);
    std::cout << "CRC32 Benchmark: CrcUtil (Castagnoli)" << std::endl;
    run_single(Algorithm::CrcUtil_C);
    std::cout << "CRC32 Benchmark: CrcUtil (IEEE)" << std::endl;
    run_single(Algorithm::CrcUtil_I);
}

void Benchmark::run_single(Algorithm algorithm)
{
    const size_t bytes_ = size_ * cycles_;

    auto start = std::chrono::system_clock::now();
    uint32_t crc32 = compute_crc32(algorithm);
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - start);
    const float rate =
        static_cast<float>(bytes_) / static_cast<float>(duration.count());
    std::cout << "crc32=" << std::hex << crc32 << "  " << rate << " MiB/s"
              << std::endl;
}
