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
#include "log.hpp"
#include <csignal>

namespace {
volatile sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  try {
    Parameters par(argc, argv);
    Application app(par, &signal_status);
    app.run();
  } catch (std::exception const& e) {
    L_(fatal) << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
