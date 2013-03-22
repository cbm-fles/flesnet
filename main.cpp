/**
 * \file main.cpp
 *
 * 2012, 2013, Jan de Cuveland
 */

/**
 * \mainpage InfiniBand-based Timeslice Building
 *
 * \section intro_sec Introduction
 *
 * The First-Level Event Selector (FLES) system of the CBM experiment
 * employs a scheme of timeslices (consisting of microslices) instead
 * of events in data aquisition. This software demonstrates the
 * timeslice building process between several nodes of the FLES
 * cluster over an Infiniband network.
 */

#include <cstdlib>
#include "Application.hpp"
#include "global.hpp"

einhard::Logger<(einhard::LogLevel) MINLOGLEVEL, true> out(einhard::WARN, true);
std::unique_ptr<Parameters> par;

int
main(int argc, char* argv[])
{
    std::unique_ptr<Application> _app;

    try {
        std::unique_ptr<Parameters> parameters(new Parameters(argc, argv));
        par = std::move(parameters);

        if (par->node_type() == Parameters::INPUT_NODE) {
            std::unique_ptr<Application> app(new InputApplication(*par));
            _app = std::move(app);
            _app->run();
        } else {
            std::unique_ptr<Application> app(new ComputeApplication(*par));
            _app = std::move(app);
            _app->run();
        }
    } catch (std::exception const& e) {
        out.fatal() << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
