// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "ChildProcessManager.hpp"
#include "Parameters.hpp"
#include "log.hpp"
#include <boost/algorithm/string.hpp>

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
  try {
    Parameters par(argc, argv);
    Application app(par);
    if (!par.exec.empty()) {
      start_exec(par.exec, par.output_shm);
      ChildProcessManager::get().allow_stop_processes(nullptr);
    }
    app.run();
  } catch (std::exception const& e) {
    L_(fatal) << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
