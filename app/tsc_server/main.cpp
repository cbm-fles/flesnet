// Copyright 2025 Dirk Hutter, Jan de Cuveland

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
    FATAL("{}", e.what());
    return EXIT_FAILURE;
  }

  INFO("Exiting");
  return EXIT_SUCCESS;
}
