// Copyright 2015 Dirk Hutter

#include "ChildProcessManager.hpp"
#include "log.hpp"
#include "parameters.hpp"
#include "shm_device_server.hpp"
#include <boost/algorithm/string.hpp>
#include <csignal>

namespace {
volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

void start_exec(const std::string& executable,
                const std::string& shared_memory_identifier) {
  assert(!executable.empty());
  ChildProcess cp = ChildProcess();
  boost::split(cp.arg, executable, boost::is_any_of(" \t"),
               boost::token_compress_on);
  cp.path = cp.arg.at(0);
  for (auto& arg : cp.arg) {
    boost::replace_all(arg, "%s", shared_memory_identifier);
  }
  ChildProcessManager::get().start_process(cp);
}

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
    flib_shm_device_server server(
        flib.get(), par.shm(), par.data_buffer_size_exp(),
        par.desc_buffer_size_exp(), par.etcd(), &signal_status);
    if (!par.exec().empty()) {
      start_exec(par.exec(), par.shm());
      ChildProcessManager::get().allow_stop_processes(nullptr);
    }
    server.run();

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
