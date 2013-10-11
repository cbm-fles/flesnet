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

#include "InputNodeApplication.hpp"
#include "ComputeNodeApplication.hpp"

#include "global.hpp"

einhard::Logger<static_cast<einhard::LogLevel>(MINLOGLEVEL), true>
out(einhard::WARN, true);
std::unique_ptr<Parameters> par;
std::vector<pid_t> child_pids;

int main(int argc, char* argv[])
{
    std::unique_ptr<InputNodeApplication> _input_app;
    std::unique_ptr<ComputeNodeApplication> _compute_app;

    try
    {
        std::unique_ptr<Parameters> parameters(new Parameters(argc, argv));
        par = std::move(parameters);

        if (!par->compute_indexes().empty()) {
            _compute_app = std::unique_ptr<ComputeNodeApplication>(
                new ComputeNodeApplication(*par, par->compute_indexes()));
            _compute_app->start();
        }

        if (!par->input_indexes().empty()) {
            _input_app = std::unique_ptr<InputNodeApplication>(
                new InputNodeApplication(*par, par->input_indexes()));
            _input_app->start();
        }

        if (_input_app)
            _input_app->join();

        if (_compute_app)
            _compute_app->join();
    }
    catch (std::exception const& e)
    {
        out.fatal() << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
