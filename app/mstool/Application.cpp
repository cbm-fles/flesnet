// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "EmbeddedPatternGenerator.hpp"
#include "FlibPatternGenerator.hpp"
#include "MicrosliceAnalyzer.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "MicrosliceReceiver.hpp"
#include "log.hpp"
#include <iostream>

Application::Application(Parameters const& par) : par_(par)
{
    if (par_.use_pattern_generator()) {
        constexpr uint32_t typical_content_size = 10000;
        constexpr std::size_t desc_buffer_size_exp = 7;  // 128 entries
        constexpr std::size_t data_buffer_size_exp = 20; // 1 MiB

        switch (par_.pattern_generator_type()) {
        case 1:
            data_source_.reset(new FlibPatternGenerator(data_buffer_size_exp,
                                                        desc_buffer_size_exp, 0,
                                                        typical_content_size));
            break;
        case 2:
            data_source_.reset(new EmbeddedPatternGenerator(
                data_buffer_size_exp, desc_buffer_size_exp, 1,
                typical_content_size));
            break;
        default:
            throw std::runtime_error("pattern generator type not available");
        }
    }

    if (data_source_) {
        source_.reset(new fles::MicrosliceReceiver(*data_source_));
    } else if (!par_.input_archive().empty()) {
        source_.reset(new fles::MicrosliceInputArchive(par_.input_archive()));
    }

    if (par_.analyze()) {
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new MicrosliceAnalyzer(10000, std::cout, "")));
    }

    if (!par_.output_archive().empty()) {
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new fles::MicrosliceOutputArchive(par_.output_archive())));
    }
}

Application::~Application()
{
    L_(info) << "total microslices processed: " << count_;
}

void Application::run()
{
    uint64_t limit = par_.maximum_number();

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
