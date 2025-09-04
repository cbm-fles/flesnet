// Copyright 2025 Jan de Cuveland

#include "Application.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : m_par(par), m_signalStatus(signal_status),
      m_producerAddress("inproc://" + par.shm_id()),
      m_workerAddress("ipc://@" + par.shm_id()),
      m_itemDistributor(m_zmqContext, m_producerAddress, m_workerAddress),
      m_timesliceBuffer(
          m_zmqContext, m_producerAddress, par.shm_id(), par.buffer_size()),
      m_distributorThread(std::ref(m_itemDistributor)) {
  if (!par.monitor_uri().empty()) {
    m_monitor = std::make_unique<cbm::Monitor>(m_par.monitor_uri());
  }

  // wait a moment to allow the timeslice buffer clients to connect
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  m_tsBuilder =
      std::make_unique<TsBuilder>(m_timesliceBuffer, par.tssched_address(),
                                  par.timeout_ns(), m_monitor.get());
}

void Application::run() {
  while (*m_signalStatus == 0) {
    std::this_thread::sleep_for(100ms);
  }
}

Application::~Application() {
  m_itemDistributor.stop();
  m_distributorThread.join();

  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = 200ms;
  std::this_thread::sleep_for(destruct_delay);
}
