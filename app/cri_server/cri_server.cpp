// Copyright 2015 Dirk Hutter

#include "ChildProcessManager.hpp"
#include "log.hpp"
#include "parameters.hpp"
#include "shm_device_server.hpp"
#include <boost/algorithm/string.hpp>
#include <csignal>
#include <memory>

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

    std::unique_ptr<cri::cri_device> cri;
    if (par.dev_autodetect()) {
      cri = std::make_unique<cri::cri_device>(0);
    } else {
      cri = std::make_unique<cri::cri_device>(
          par.dev_addr().bus, par.dev_addr().dev, par.dev_addr().func);
    }
    L_(info) << "using CRI: " << cri->print_devinfo();

    // create server
    cri_shm_device_server server(cri.get(), par.shm(),
                                 par.data_buffer_size_exp(),
                                 par.desc_buffer_size_exp(), &signal_status);
    if (!par.exec().empty()) {
      start_exec(par.exec(), par.shm());
      ChildProcessManager::get().allow_stop_processes(nullptr);
    }
    server.run();

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
    return EXIT_FAILURE;
  }

  L_(info) << "exiting";
  return EXIT_SUCCESS;
}
