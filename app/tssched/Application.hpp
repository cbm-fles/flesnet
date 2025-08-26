// Copyright 2025 Jan de Cuveland
#pragma once

#include "Monitor.hpp"
#include "Parameters.hpp"
#include "Scheduler.hpp"
#include "TsScheduler.hpp"
#include <csignal>
#include <memory>

/// %Application base class.
class Application {
public:
  Application(Parameters const& par, volatile sig_atomic_t* signal_status);

  Application(const Application&) = delete;
  void operator=(const Application&) = delete;

  ~Application();

  void run();

private:
  Parameters const& par_;
  volatile std::sig_atomic_t* signal_status_;

  std::unique_ptr<cbm::Monitor> monitor_;

  Scheduler tasks_;

  std::unique_ptr<TsScheduler> ts_scheduler_;
};
