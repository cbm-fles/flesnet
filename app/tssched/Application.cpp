// Copyright 2025 Jan de Cuveland

#include "Application.hpp"
#include "System.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace {
[[maybe_unused]] inline uint64_t
chrono_to_timestamp(std::chrono::time_point<std::chrono::high_resolution_clock,
                                            std::chrono::nanoseconds> time) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             time.time_since_epoch())
      .count();
}

[[maybe_unused]] inline std::chrono::
    time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds>
    timestamp_to_chrono(uint64_t time) {
  return std::chrono::high_resolution_clock::time_point(
      std::chrono::nanoseconds(time));
}

} // namespace

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status) {
  if (!par.monitor_uri().empty()) {
    monitor_ = std::make_unique<cbm::Monitor>(par_.monitor_uri());
  }
  hostname_ = fles::system::current_hostname();

  ts_scheduler_ = std::make_unique<TsScheduler>(par.listen_port());
}

void Application::run() {
  uint64_t ts_start_time =
      chrono_to_timestamp(std::chrono::high_resolution_clock::now()) /
      par_.timeslice_duration_ns() * par_.timeslice_duration_ns();
  // acked_ = ts_start_time / par_.timeslice_duration_ns();

  report_status();

  while (*signal_status_ == 0) {
    scheduler_.timer();
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
    // monitor_->QueueMetric(...)
  }

  scheduler_.add([this] { report_status(); }, now + interval);
}