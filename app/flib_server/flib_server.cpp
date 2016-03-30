// Copyright 2015 Dirk Hutter

#include "log.hpp"
#include "parameters.hpp"
#include "shm_device_server.hpp"
#include <csignal>

namespace {
volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

int main(int argc, char* argv[]) {

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  try {

    parameters par(argc, argv);

    std::unique_ptr<flib::flib_device> flib;
    L_(info) << "initializing FLIB with DPB readout";
    flib = std::unique_ptr<flib::flib_device>(new flib::flib_device_flesin(0));

    // create server
    flib_shm_device_server server(flib.get(), par.data_buffer_size_exp(),
                                  par.desc_buffer_size_exp(), &signal_status);
    server.run();

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
  }

  return 0;
}
