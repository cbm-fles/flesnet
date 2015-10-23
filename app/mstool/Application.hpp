// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DualRingBuffer.hpp"
#include "MicrosliceSource.hpp"
#include "Parameters.hpp"
#include "Sink.hpp"
#include "shm_device_client.hpp"
#include "shm_device_provider.hpp"
#include <memory>
#include <vector>

/// %Application base class.
class Application
{
public:
    explicit Application(Parameters const& par);

    Application(const Application&) = delete;
    void operator=(const Application&) = delete;

    ~Application();

    void run();

private:
    Parameters const& par_;

    std::unique_ptr<InputBufferReadInterface> pattern_generator_;
    std::unique_ptr<flib_shm_device_client> shm_device_;
    std::unique_ptr<flib_shm_device_provider> output_shm_device_;
    InputBufferReadInterface* data_source_;

    std::unique_ptr<fles::MicrosliceSource> source_;
    std::vector<std::unique_ptr<fles::MicrosliceSink>> sinks_;

    uint64_t count_ = 0;
};
