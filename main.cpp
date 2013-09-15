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

#include "include.hpp"

#include "global.hpp"

#include "Timeslice.hpp"
#include "Parameters.hpp"
#include "RingBuffer.hpp"
#include "IBConnection.hpp"
#include "IBConnectionGroup.hpp"
#include "InputNodeConnection.hpp"
#include "DataSource.hpp"
#include "InputBuffer.hpp"
#include "ComputeNodeConnection.hpp"
#include "concurrent_queue.hpp"
#include "ComputeBuffer.hpp"
#include "TimesliceProcessor.hpp"
#include "Application.hpp"

einhard::Logger<(einhard::LogLevel) MINLOGLEVEL, true> out(einhard::WARN, true);
std::unique_ptr<Parameters> par;
std::vector<pid_t> child_pids;

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
        } else {
            std::unique_ptr<Application> app(new ComputeApplication(*par));
            _app = std::move(app);
        }
        _app->run();
    } catch (std::exception const& e) {
        out.fatal() << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
