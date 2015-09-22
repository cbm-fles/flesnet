// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "RingBufferReadInterface.hpp"
#include "MicrosliceSource.hpp"
#include "Parameters.hpp"
#include "Sink.hpp"
#include "shm_device_client.hpp"
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

    std::unique_ptr<InputBufferReadInterface> data_source_;
    std::unique_ptr<fles::MicrosliceSource> source_;
    std::vector<std::unique_ptr<fles::MicrosliceSink>> sinks_;

    uint64_t count_ = 0;
    std::unique_ptr<shm_device_client> shm_device_;
};
