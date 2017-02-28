// Copyright 2015 Dirk Hutter

#include "EtcdClient.h"
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
    if (par.flib_autodetect()) {
      flib = std::unique_ptr<flib::flib_device_flesin>(
          new flib::flib_device_flesin(0));
    } else {
      flib = std::unique_ptr<flib::flib_device_flesin>(
          new flib::flib_device_flesin(par.flib_addr().bus, par.flib_addr().dev,
                                       par.flib_addr().func));
    }
    L_(info) << "using FLIB: " << flib->print_devinfo();

    // create server
    flib_shm_device_server server(flib.get(), par.shm(),
                                  par.data_buffer_size_exp(),
                                  par.desc_buffer_size_exp(), &signal_status);

    EtcdClient etcd(par.base_url());

    if (par.kv_sync()) {
      L_(info) << "Now using key-value store for synchronization at "
               << par.base_url();
      int ret = etcd.set_value("/" + par.shm() + "/uptodate", "value=on");
      if (ret != 0)
        throw std::runtime_error("Error setting value in key-value store");
    }

    server.run();

    if (par.kv_sync())
      etcd.set_value("/" + par.shm() + "/uptodate", "value=off");

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
