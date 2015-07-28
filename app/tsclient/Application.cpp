// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceInputArchive.hpp"
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
        debug_.reset(new TimesliceDebugger());

    if (!par_.output_archive().empty())
        output_.reset(new fles::TimesliceOutputArchive(par_.output_archive()));

    if (!par_.publish_address().empty())
        publisher_.reset(new fles::TimeslicePublisher(par_.publish_address()));

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
        if (debug_) {
            std::cout << debug_->dump_timeslice(*timeslice, par_.verbosity())
                      << std::endl;
        }
        if (output_)
            output_->write(*timeslice);
        if (publisher_)
            publisher_->publish(*timeslice);
        ++count_;
    }
}
