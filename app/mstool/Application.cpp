// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "EmbeddedPatternGenerator.hpp"
#include "FlibPatternGenerator.hpp"
#include "shm_channel_client.hpp"
#include "MicrosliceAnalyzer.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "MicrosliceReceiver.hpp"
#include "MicrosliceTransmitter.hpp"
#include "log.hpp"
#include <iostream>

Application::Application(Parameters const& par) : par_(par)
{
    // Source setup
    if (par_.use_shared_memory) {
        L_(info) << "using shared memory as data source";

        shm_device_.reset(new flib_shm_device_client("flib_shared_memory"));

        if (par_.shared_memory_channel < shm_device_->num_channels()) {
            data_source_ =
                shm_device_->channels().at(par.shared_memory_channel);
        } else {
            throw std::runtime_error("shared memory channel not available");
        }
    } else if (par_.use_pattern_generator) {
        L_(info) << "using pattern generator as data source";

        constexpr uint32_t typical_content_size = 10000;
        constexpr std::size_t desc_buffer_size_exp = 7;  // 128 entries
        constexpr std::size_t data_buffer_size_exp = 20; // 1 MiB

        switch (par_.pattern_generator_type) {
        case 1:
            pattern_generator_.reset(new FlibPatternGenerator(
                data_buffer_size_exp, desc_buffer_size_exp, 0,
                typical_content_size));
            break;
        case 2:
            pattern_generator_.reset(new EmbeddedPatternGenerator(
                data_buffer_size_exp, desc_buffer_size_exp, 1,
                typical_content_size));
            break;
        default:
            throw std::runtime_error("pattern generator type not available");
        }

        data_source_ = pattern_generator_.get();
    }

    if (data_source_) {
        source_.reset(new fles::MicrosliceReceiver(*data_source_));
    } else if (!par_.input_archive.empty()) {
        source_.reset(new fles::MicrosliceInputArchive(par_.input_archive));
    }

    // Sink setup
    if (par_.analyze) {
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new MicrosliceAnalyzer(10000, std::cout, "")));
    }

    if (!par_.output_archive.empty()) {
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new fles::MicrosliceOutputArchive(par_.output_archive)));
    }

    if (!par_.output_shm_identifier.empty()) {
        L_(info) << "providing output in shared memory: "
                 << par_.output_shm_identifier;

        constexpr std::size_t desc_buffer_size_exp = 7;  // 128 entries
        constexpr std::size_t data_buffer_size_exp = 20; // 1 MiB

        output_shm_device_.reset(new flib_shm_device_provider(
            par_.output_shm_identifier, 1, data_buffer_size_exp,
            desc_buffer_size_exp));
        InputBufferWriteInterface* data_sink =
            output_shm_device_->channels().at(0);
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new fles::MicrosliceTransmitter(*data_sink)));
    }
}

Application::~Application()
{
    L_(info) << "total microslices processed: " << count_;
}

void Application::run()
{
    uint64_t limit = par_.maximum_number;

    while (auto microslice = source_->get()) {
        for (auto& sink : sinks_) {
            sink->put(*microslice);
        }
        ++count_;
        if (count_ == limit) {
            break;
        }
    }
}
