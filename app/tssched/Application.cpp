// Copyright 2025 Jan de Cuveland

#include "Application.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status) {
  if (!par.monitor_uri().empty()) {
    monitor_ = std::make_unique<cbm::Monitor>(par_.monitor_uri());
  }

  ts_scheduler_ = std::make_unique<TsScheduler>(
      par.listen_port(), par.timeslice_duration_ns(), par.timeout_ns(),
      monitor_.get());
}

void Application::run() {
  while (*signal_status_ == 0) {
    tasks_.timer();
    std::this_thread::sleep_until(tasks_.when_next());
  }
}

Application::~Application() {
  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = std::chrono::milliseconds(200);
  std::this_thread::sleep_for(destruct_delay);
}
