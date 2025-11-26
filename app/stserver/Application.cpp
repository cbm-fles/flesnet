/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Jan de Cuveland, Dirk Hutter */

#include "Application.hpp"

using namespace std::chrono_literals;

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : m_par(par) {
  // start up monitoring
  if (!m_par.monitor_uri().empty()) {
    m_monitor = std::make_unique<cbm::Monitor>(m_par.monitor_uri());
  }

  // Create StSender
  m_st_sender = std::make_unique<StSender>(
      m_par.tssched_address(), m_par.listen_port(), m_par.sender_info());

  // Create StBuilder
  m_st_builder = std::make_unique<StBuilder>(
      signal_status, m_monitor.get(), m_par.sender_info(), *m_st_sender,
      m_par.device_autodetect(), m_par.device_address(), m_par.shm_id(),
      m_par.pgen_channels(), m_par.pgen_microslice_duration_ns(),
      m_par.pgen_microslice_size(), m_par.pgen_flags(),
      m_par.timeslice_duration_ns(), m_par.timeout_ns(),
      m_par.data_buffer_size(), m_par.desc_buffer_size(),
      m_par.overlap_before_ns(), m_par.overlap_after_ns());
}

void Application::run() { m_st_builder->run(); }

Application::~Application() {
  m_st_builder.reset();
  m_st_sender.reset();

  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = 200ms;
  std::this_thread::sleep_for(destruct_delay);
}
