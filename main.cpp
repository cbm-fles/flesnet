/**
 * \file main.cpp
 *
 * 2012, Jan de Cuveland
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

einhard::Logger<(einhard::LogLevel) MINLOGLEVEL, true> Log(einhard::WARN, true);
Parameters* Par;

int
main(int argc, char* argv[])
{
    try {
        Par = new Parameters(argc, argv);

        if (Par->node_type() == Parameters::INPUT_NODE) {
            InputApplication app(*Par);
            app.run();
        } else {
            ComputeApplication app(*Par);
            app.run();
        }
    } catch (std::exception const& e) {
        Log.fatal() << e.what();
        return EXIT_FAILURE;
    }

    delete Par;
    return EXIT_SUCCESS;
}
