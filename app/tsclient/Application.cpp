// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "TimesliceAnalyzer.hpp"
#include "TimesliceDebugger.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimeslicePublisher.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceSubscriber.hpp"
#include <boost/lexical_cast.hpp>
#include <iostream>

Application::Application(Parameters const& par) : par_(par)
{
    if (!par_.shm_identifier().empty())
        source_.reset(new fles::TimesliceReceiver(par_.shm_identifier()));
    else if (!par_.input_archive().empty())
        source_.reset(new fles::TimesliceInputArchive(par_.input_archive()));
    else if (!par_.subscribe_address().empty())
        source_.reset(new fles::TimesliceSubscriber(par_.subscribe_address()));

    if (par_.analyze()) {
        std::string output_prefix =
            boost::lexical_cast<std::string>(par_.client_index()) + ": ";
        sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
            new TimesliceAnalyzer(10000, std::cout, output_prefix)));
    }

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

    uint64_t limit = par_.maximum_number();

    while (auto timeslice = source_->get()) {
        for (auto& sink : sinks_) {
            sink->put(*timeslice);
        }
        ++count_;
        if (count_ == limit) {
            break;
        }
    }
}
