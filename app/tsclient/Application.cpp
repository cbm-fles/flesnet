// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceSubscriber.hpp"
#include <iostream>

Application::Application(Parameters const& par) : _par(par)
{
    if (!_par.shm_identifier().empty())
        _source.reset(new fles::TimesliceReceiver(_par.shm_identifier()));
    else if (!_par.input_archive().empty())
        _source.reset(new fles::TimesliceInputArchive(_par.input_archive()));
    else
        _source.reset(new fles::TimesliceSubscriber(_par.subscribe_address()));

    if (_par.analyze())
        _analyzer.reset(new TimesliceAnalyzer());

    if (_par.verbosity() > 0)
        _debug.reset(new TimesliceDebugger());

    if (!_par.output_archive().empty())
        _output.reset(new fles::TimesliceOutputArchive(_par.output_archive()));

    if (!_par.publish_address().empty())
        _publisher.reset(new fles::TimeslicePublisher(_par.publish_address()));

    if (_par.client_index() != -1) {
        std::cout << "tsclient " << _par.client_index() << ": "
                  << par.shm_identifier() << std::endl;
    }
}

Application::~Application()
{
    if (_par.client_index() != -1) {
        std::cout << "tsclient " << _par.client_index() << ": ";
    }
    std::cout << "total timeslices processed: " << _count << std::endl;
}

void Application::run()
{
    while (auto timeslice = _source->get()) {
        if (_analyzer) {
            _analyzer->check_timeslice(*timeslice);
            if ((_analyzer->count() % 10000) == 0) {
                std::cout << _par.client_index() << ": "
                          << _analyzer->statistics() << std::endl;
                _analyzer->reset();
            }
        }
        if (_debug) {
            std::cout << _debug->dump_timeslice(*timeslice, _par.verbosity())
                      << std::endl;
        }
        if (_output)
            _output->write(*timeslice);
        if (_publisher)
            _publisher->publish(*timeslice);
        ++_count;
    }
}
