// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

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

#include "Application.hpp"
#include "Parameters.hpp"
#include <csignal>
#include <log.hpp>

namespace
{
volatile sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

int main(int argc, char* argv[])
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        Parameters par(argc, argv);
        if (par.use_libfabric() ==
            par.use_rdma()) { // (!par.use_libfabric() && !par.use_rdma()) ||
                              // (par.use_libfabric() && par.use_rdma())
            L_(fatal) << "use-libfabric = " << par.use_libfabric()
                      << ", use-rdma = " << par.use_rdma()
                      << ". Only one of the following flags must be true in "
                         "the configuration file: {use-libfabric, use-rdma}";
            return EXIT_FAILURE;
        }
        Application app(par, &signal_status);
        app.run();
    } catch (std::exception const& e) {
        L_(fatal) << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
