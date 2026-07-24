// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "Parameters.hpp"
#include "log.hpp"
#include <csignal>
#include <cstdlib>

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
  } catch (ParametersException const& e) {
    std::cerr << "Parameter error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (std::exception const& e) {
    L_(fatal) << "Unknown exception: " << e.what();
    std::abort();
  }

  L_(info) << "exiting";
  return EXIT_SUCCESS;
}
