// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <vector>
#include <chrono>

/// %Benchmark class.
class Benchmark
{
public:
    Benchmark();
    void run();

    enum class Algorithm {
        Boost_C,
        Boost_I,
        Intrinsic32,
        Intrinsic64,
        CrcUtil_C,
        CrcUtil_I
    };
    uint32_t compute_crc32(Algorithm algorithm);
    void run_single(Algorithm algorithm);

    const size_t _size = 1048576;
    const size_t _cycles = 500;

private:
    std::vector<uint8_t> _random_data;
};
