// Copyright 2025 Jan de Cuveland
#pragma once

#include "Monitor.hpp"
#include "Parameters.hpp"
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
  Parameters const& m_par;
  volatile std::sig_atomic_t* m_signal_status;

  std::unique_ptr<cbm::Monitor> m_monitor;

  std::unique_ptr<TsScheduler> m_ts_scheduler;
};