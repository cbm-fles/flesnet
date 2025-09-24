/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "Application.hpp"
#include <thread>

using namespace std::chrono_literals;

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : m_par(par) {
  if (!par.monitor_uri().empty()) {
    m_monitor = std::make_unique<cbm::Monitor>(m_par.monitor_uri());
  }

  m_ts_scheduler = std::make_unique<TsScheduler>(
      signal_status, par.listen_port(), par.timeslice_duration_ns(),
      par.timeout_ns(), m_monitor.get());
}

void Application::run() { m_ts_scheduler->run(); }

Application::~Application() {
  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = 200ms;
  std::this_thread::sleep_for(destruct_delay);
}
