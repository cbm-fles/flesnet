// Copyright 2015 Dirk Hutter

#include <csignal>

#include "log.hpp"

#include "parameters.hpp"
#include "shm_device_server.hpp"

namespace {
volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

int main(int argc, char* argv[]) {

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  try {

    parameters par(argc, argv);

    std::unique_ptr<flib_device> flib =
        std::unique_ptr<flib_device>(new flib_device_cnet(0));

    // convert sizes from 'entries' to 'bytes'
    size_t data_buffer_size_bytes_exp = par.data_buffer_size_exp();
    constexpr std::size_t microslice_descriptor_size_bytes_exp = 5;
    size_t desc_buffer_size_bytes_exp =
        par.desc_buffer_size_exp() + microslice_descriptor_size_bytes_exp;

    // create server
    shm_device_server server(flib.get(),
                             data_buffer_size_bytes_exp,
                             desc_buffer_size_bytes_exp,
                             &signal_status);
    server.run();

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
  }

  return 0;
}
