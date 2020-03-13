// Copyright 2012-2013, 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "log.hpp"
#include <csignal>
#include <cstring>
#include <sstream>
#include <sys/wait.h>
#include <vector>

enum ProcessStatus { None, Running, Terminating };

struct ChildProcess {
  pid_t pid;
  std::string path;
  std::vector<std::string> arg;
  void* owner;
  ProcessStatus status;
};

class ChildProcessManager {
public:
  static ChildProcessManager& get() {
    static ChildProcessManager instance;
    return instance;
  }

  bool start_process(ChildProcess child_process) {
    std::stringstream args;
    copy(child_process.arg.begin(), child_process.arg.end(),
         std::ostream_iterator<std::string>(args, " "));
    L_(debug) << "starting " << child_process.path << " with args "
              << args.str();

    std::vector<const char*> c_arg;
    std::transform(child_process.arg.begin(), child_process.arg.end(),
                   std::back_inserter(c_arg),
                   [](std::string& s) { return s.c_str(); });
    c_arg.push_back(nullptr);

    pid_t pid = vfork();
    if (pid == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
      // child
      std::signal(SIGINT, SIG_IGN);
      std::signal(SIGTERM, SIG_IGN);
#pragma GCC diagnostic pop
      execvp(child_process.path.c_str(), const_cast<char* const*>(&c_arg[0]));
      L_(error) << "execvp() failed: " << strerror(errno);
      _exit(0);
    } else {
      // parent
      if (pid > 0) {
        child_process.pid = pid;
        child_process.status = Running;
        child_processes_.push_back(child_process);
        L_(debug) << "child process started";
        return true;
      }
      L_(error) << "vfork() failed: " << strerror(errno);
      return false;
    }
  }

  bool stop_process(std::vector<ChildProcess>::iterator child_process) {
    if (child_process >= child_processes_.begin() &&
        child_process < child_processes_.end()) {
      child_process->status = Terminating;
      kill(child_process->pid, SIGTERM);
      return true;
    }
    return false;
  }

  void stop_processes(void* owner) {
    for (auto it = child_processes_.begin(); it < child_processes_.end();
         ++it) {
      if (it->owner == owner) {
        stop_process(it);
      }
    }
  }

  bool allow_stop_process(std::vector<ChildProcess>::iterator child_process) {
    if (child_process >= child_processes_.begin() &&
        child_process < child_processes_.end()) {
      child_process->status = Terminating;
      return true;
    }
    return false;
  }

  void allow_stop_processes(void* owner) {
    for (auto it = child_processes_.begin(); it < child_processes_.end();
         ++it) {
      if (it->owner == owner) {
        allow_stop_process(it);
      }
    }
  }

  void stop_all_processes() {
    for (auto it = child_processes_.begin(); it < child_processes_.end();
         ++it) {
      stop_process(it);
    }
  }

private:
  using sigaction_struct = struct sigaction;

  ChildProcessManager() { install_sigchld_handler(); }

  ~ChildProcessManager() {
    uninstall_sigchld_handler();
    stop_all_processes();
  }

  void uninstall_sigchld_handler() { sigaction(SIGCHLD, &oldact_, nullptr); }

  void install_sigchld_handler() {
    sigaction_struct sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, &oldact_);
  }

  static void sigchld_handler(int sig) {
    assert(sig == SIGCHLD);

    pid_t pid;
    int status;

    std::vector<ChildProcess>& child_processes =
        ChildProcessManager::get().child_processes();

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      // process with given pid has exited
      bool pid_found = false;
      std::size_t idx = 0;
      for (std::size_t i = 0; i != child_processes.size(); ++i) {
        if (child_processes[i].pid == pid) {
          pid_found = true;
          idx = i;
          break;
        }
      }
      if (!pid_found) {
        L_(error) << "unknown child process died";
      } else {
        switch (child_processes.at(idx).status) {
        case Terminating:
          L_(debug) << "child process successfully terminated";
          break;
        case Running:
        case None:
          L_(error) << "child process died unexpectedly";
          L_(error) << "TODO: restart not yet implemented";
          // TODO(Jan): Implement child process restarting
          break;
        }
      }
    }
  }

  std::vector<ChildProcess>& child_processes() { return child_processes_; }

  std::vector<ChildProcess> child_processes_;

  sigaction_struct oldact_ = sigaction_struct();
};
