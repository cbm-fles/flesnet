/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "Application.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : m_par(par), m_producer_address("inproc://" + par.shm_id()),
      m_worker_address("ipc://@" + par.shm_id()),
      m_item_distributor(m_zmq_context, m_producer_address, m_worker_address),
      m_timeslice_buffer(
          m_zmq_context, m_producer_address, par.shm_id(), par.buffer_size()),
      m_distributor_thread(std::ref(m_item_distributor)) {
  if (!par.monitor_uri().empty()) {
    m_monitor = std::make_unique<cbm::Monitor>(m_par.monitor_uri());
  }

  // wait a moment to allow the timeslice buffer clients to connect
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  m_ts_builder = std::make_unique<TsBuilder>(signal_status, m_timeslice_buffer,
                                             par.tssched_address(),
                                             par.timeout_ns(), m_monitor.get());
}

void Application::run() { m_ts_builder->run(); }

Application::~Application() {
  m_item_distributor.stop();
  m_distributor_thread.join();

  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = 200ms;
  std::this_thread::sleep_for(destruct_delay);
}
