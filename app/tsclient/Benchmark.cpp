// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "Benchmark.hpp"
#include <boost/crc.hpp>
#include <random>
#include <iostream>
#include <smmintrin.h>

Benchmark::Benchmark()
{
    _random_data.reserve(_size);

    std::mt19937 engine;
    std::uniform_int_distribution<uint8_t> distribution;
    auto generator = std::bind(distribution, engine);

    std::generate_n(std::back_inserter(_random_data), _size, generator);
}

uint32_t Benchmark::compute_crc32(Algorithm algorithm)
{
    uint32_t crc = 0;

    switch (algorithm) {

    case Algorithm::Boost: {
        boost::crc_32_type crc_32;

        for (size_t i = 0; i < _cycles; ++i) {
            crc_32.process_bytes(_random_data.data(), _random_data.size());
        }
        crc = crc_32();
        break;
    }

    case Algorithm::Intrinsic32: {
        for (size_t i = 0; i < _cycles; ++i) {
            uint32_t* p = reinterpret_cast<uint32_t*>(_random_data.data());
            const uint32_t* const end =
                p + _random_data.size() / sizeof(uint32_t);
            while (p < end) {
                crc = _mm_crc32_u32(crc, *p++);
            }
        }
        break;
    }

    case Algorithm::Intrinsic64: {
        uint64_t crc64 = 0;
        for (size_t i = 0; i < _cycles; ++i) {
            uint64_t* p = reinterpret_cast<uint64_t*>(_random_data.data());
            const uint64_t* const end =
                p + _random_data.size() / sizeof(uint64_t);
            while (p < end) {
                crc64 = _mm_crc32_u64(crc64, *p++);
            }
        }
        crc = static_cast<uint32_t>(crc64 & 0xffffffffU);
        break;
    }
    }

    return crc;
}

void Benchmark::run()
{
    std::cout << "CRC32 Benchmark: Boost" << std::endl;
    run_single(Algorithm::Boost);
    std::cout << "CRC32 Benchmark: Intrinsic32" << std::endl;
    run_single(Algorithm::Intrinsic32);
    std::cout << "CRC32 Benchmark: Intrinsic64" << std::endl;
    run_single(Algorithm::Intrinsic64);
}

void Benchmark::run_single(Algorithm algorithm)
{
    const size_t _bytes = _size * _cycles;

    auto start = std::chrono::system_clock::now();
    uint32_t crc32 = compute_crc32(algorithm);
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - start);
    const float rate =
        static_cast<float>(_bytes) / static_cast<float>(duration.count());
    std::cout << "crc32=" << std::hex << crc32 << "\n";
    std::cout << rate << " MiB/s" << std::endl;
}
