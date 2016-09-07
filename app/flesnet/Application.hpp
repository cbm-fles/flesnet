// Copyright 2012-2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "InputChannelSender.hpp"
#include "Parameters.hpp"
#include "ThreadContainer.hpp"
#include "TimesliceBuffer.hpp"
#include "TimesliceBuilder.hpp"
#include "shm_device_client.hpp"
#include <boost/lexical_cast.hpp>
#include <csignal>
#include <memory>

/// %Application base class.
/** The Application object represents an instance of the running
    application. */

class Application : public ThreadContainer
{
public:
    /// The Application contructor.
    explicit Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status);

    /// The Application destructor.
    ~Application();

    void run();

    Application(const Application&) = delete;
    void operator=(const Application&) = delete;

private:
    /// The run parameters object.
    Parameters const& par_;
    volatile sig_atomic_t* signal_status_;

    // Input node application
    std::shared_ptr<flib_shm_device_client> shm_device_;
    std::size_t shm_num_channels_ = 0;

    std::vector<std::unique_ptr<InputBufferReadInterface>> data_sources_;

    /// The application's connection group / buffer objects
    std::vector<std::unique_ptr<TimesliceBuffer>> timeslice_buffers_;
    std::vector<std::unique_ptr<TimesliceBuilder>> timeslice_receivers_;
    std::vector<std::unique_ptr<InputChannelSender>> input_channel_senders_;

    void start_processes(const std::string shared_memory_identifier);
};
