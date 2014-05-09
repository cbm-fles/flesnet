// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceInputArchive.hpp"
#include <iostream>

Application::Application(Parameters const& par) : _par(par)
{
    if (!_par.shm_identifier().empty())
        _source.reset(new fles::TimesliceReceiver(_par.shm_identifier()));
    else
        _source.reset(new fles::TimesliceInputArchive(_par.input_archive()));

    if (_par.analyze())
        _analyzer.reset(new TimesliceAnalyzer());

    if (_par.dump())
        _dump.reset(new TimesliceDebugger());

    if (!_par.output_archive().empty())
        _output.reset(new fles::TimesliceOutputArchive(_par.output_archive()));

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
        if (_dump) {
            std::cout << _dump->dump_timeslice(*timeslice) << std::endl;
        }
        if (_output)
            _output->write(*timeslice);
        ++_count;
    }
}
