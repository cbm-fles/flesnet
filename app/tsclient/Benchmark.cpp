// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "Benchmark.hpp"
#include <boost/crc.hpp>
#include <random>
#include <iostream>

Benchmark::Benchmark()
{
    _random_data.reserve(_size);

    std::mt19937 engine;
    std::uniform_int_distribution<uint8_t> distribution;
    auto generator = std::bind(distribution, engine);

    std::generate_n(std::back_inserter(_random_data), _size, generator);
}

uint32_t Benchmark::crc32_boost()
{
    boost::crc_32_type crc_32;

    for (size_t i = 0; i < _cycles; ++i) {
        crc_32.process_bytes(_random_data.data(), _random_data.size());
    }
    return crc_32();
}

void Benchmark::run()
{
    const size_t _bytes = _size * _cycles;

    auto start = std::chrono::system_clock::now();
    uint32_t crc32 = crc32_boost();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - start);
    const float rate =
        static_cast<float>(_bytes) / static_cast<float>(duration.count());
    std::cout << "crc32=" << std::hex << crc32 << "\n";
    std::cout << rate << " MiB/s" << std::endl;
}
