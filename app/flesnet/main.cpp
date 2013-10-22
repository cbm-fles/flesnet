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

#include "Parameters.hpp"
#include "Application.hpp"

#include "global.hpp"

einhard::Logger<static_cast<einhard::LogLevel>(MINLOGLEVEL), true>
out(einhard::WARN, true);

int main(int argc, char* argv[])
{
    try
    {
        Parameters par(argc, argv);
        Application app(par);
        app.run();
    }
    catch (std::exception const& e)
    {
        out.fatal() << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
