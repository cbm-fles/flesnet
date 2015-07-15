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

    uint32_t crc32_boost();

    const size_t _size = 1048576;
    const size_t _cycles = 500;

private:
    std::vector<uint8_t> _random_data;
};
