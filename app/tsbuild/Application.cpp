// Copyright 2025 Jan de Cuveland

#include "Application.hpp"
#include "System.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status),
      producer_address_("inproc://" + par.shm_id()),
      worker_address_("ipc://@" + par.shm_id()),
      item_distributor_(zmq_context_, producer_address_, worker_address_),
      timeslice_buffer_(
          zmq_context_, producer_address_, par.shm_id(), par.buffer_size()),
      distributor_thread_(std::ref(item_distributor_)) {
  if (!par.monitor_uri().empty()) {
    monitor_ = std::make_unique<cbm::Monitor>(par_.monitor_uri());
  }

  // wait a moment to allow the timeslice buffer clients to connect
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ts_builder_ =
      std::make_unique<TsBuilder>(timeslice_buffer_, par.tssched_address(),
                                  par.timeout_ns(), monitor_.get());
}

void Application::run() {
  while (*signal_status_ == 0) {
    scheduler_.timer();
    std::this_thread::sleep_until(scheduler_.when_next());
  }
}

Application::~Application() {
  item_distributor_.stop();
  distributor_thread_.join();

  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = std::chrono::milliseconds(200);
  std::this_thread::sleep_for(destruct_delay);
}
