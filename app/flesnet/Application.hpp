// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ThreadContainer.hpp"
#include "Parameters.hpp"
#include "ComputeBuffer.hpp"
#include "InputChannelSender.hpp"
#include <flib.h>
#include <boost/lexical_cast.hpp>
#include <memory>
#include <csignal>
#include "shm_device_client.hpp"

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
    std::unique_ptr<flib::flib_device> flib_;
    std::vector<flib::flib_link*> flib_links_;

    std::unique_ptr<shm_device_client> shm_device_;
    std::size_t shm_num_channels_ = 0;

    std::vector<std::unique_ptr<InputBufferReadInterface>> data_sources_;

    /// The application's connection group / buffer objects
    std::vector<std::unique_ptr<ComputeBuffer>> compute_buffers_;
    std::vector<std::unique_ptr<InputChannelSender>> input_channel_senders_;
};
