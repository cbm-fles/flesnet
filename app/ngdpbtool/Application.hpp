// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Pierre-Alain Loizeau <p.-a.loizeau@gsi.de>
#pragma once

#include "NdpbEpochToMsSorter.hpp"
#include "MicrosliceTransmitter.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "DualRingBuffer.hpp"
#include "MicrosliceSource.hpp"
#include "Parameters.hpp"
#include "Sink.hpp"
#include "shm_device_client.hpp"
#include "shm_device_provider.hpp"
#include <memory>
#include <vector>
#include <chrono>

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

    std::shared_ptr<flib_shm_device_client> shm_device_;
    std::unique_ptr<flib_shm_device_provider> output_shm_device_;
    std::unique_ptr<InputBufferReadInterface> data_source_;

    fles::MicrosliceOutputArchive* filtered_output_archive_ = NULL;
    fles::NdpbEpochToMsSorter*     ngdpb_sorter_arch_       = NULL;
    fles::MicrosliceTransmitter*   filtered_output_mstrans_ = NULL;
    fles::NdpbEpochToMsSorter*     ngdpb_sorter_shm_        = NULL;

    std::unique_ptr<fles::MicrosliceSource> source_;
    std::vector<std::unique_ptr<fles::MicrosliceSink>> sinks_;

    uint64_t count_ = 0;
    std::chrono::steady_clock::time_point tStart;
};
