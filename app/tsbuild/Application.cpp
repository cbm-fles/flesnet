// Copyright 2025 Jan de Cuveland

#include "Application.hpp"
#include "System.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status) {
  if (!par.monitor_uri().empty()) {
    monitor_ = std::make_unique<cbm::Monitor>(par_.monitor_uri());
  }
  hostname_ = fles::system::current_hostname();

  ts_builder_ =
      std::make_unique<TsBuilder>(par.tssched_address(), par.timeout_ns());
}

void Application::run() {
  report_status();

  while (*signal_status_ == 0) {
    scheduler_.timer();
    std::this_thread::sleep_until(scheduler_.when_next());
  }
}

Application::~Application() {
  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = std::chrono::milliseconds(200);
  std::this_thread::sleep_for(destruct_delay);
}

void Application::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  int64_t now_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();

  if (monitor_ != nullptr) {
    // TODO: Report scheduler status
    // monitor_->QueueMetric(...)
  }

  scheduler_.add([this] { report_status(); }, now + interval);
}