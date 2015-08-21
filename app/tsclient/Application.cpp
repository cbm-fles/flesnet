// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "TimesliceDebugger.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimeslicePublisher.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceSubscriber.hpp"
#include <iostream>

Application::Application(Parameters const& par) : par_(par)
{
    if (!par_.shm_identifier().empty())
        source_.reset(new fles::TimesliceReceiver(par_.shm_identifier()));
    else if (!par_.input_archive().empty())
        source_.reset(new fles::TimesliceInputArchive(par_.input_archive()));
    else if (!par_.subscribe_address().empty())
        source_.reset(new fles::TimesliceSubscriber(par_.subscribe_address()));

    if (par_.analyze())
        analyzer_.reset(new TimesliceAnalyzer());

    if (par_.verbosity() > 0)
        sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
            new TimesliceDumper(std::cout, par_.verbosity())));

    if (!par_.output_archive().empty())
        sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
            new fles::TimesliceOutputArchive(par_.output_archive())));

    if (!par_.publish_address().empty())
        sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
            new fles::TimeslicePublisher(par_.publish_address())));

    if (par_.benchmark())
        benchmark_.reset(new Benchmark());

    if (par_.client_index() != -1) {
        std::cout << "tsclient " << par_.client_index() << ": "
                  << par.shm_identifier() << std::endl;
    }
}

Application::~Application()
{
    if (par_.client_index() != -1) {
        std::cout << "tsclient " << par_.client_index() << ": ";
    }
    std::cout << "total timeslices processed: " << count_ << std::endl;
}

void Application::run()
{
    if (benchmark_) {
        benchmark_->run();
        return;
    }

    while (auto timeslice = source_->get()) {
        if (analyzer_) {
            analyzer_->check_timeslice(*timeslice);
            if ((analyzer_->count() % 10000) == 0) {
                std::cout << par_.client_index() << ": "
                          << analyzer_->statistics() << std::endl;
                analyzer_->reset();
            }
        }
        for (auto& sink : sinks_) {
            sink->put(*timeslice);
        }
        ++count_;
    }
}
