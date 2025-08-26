// Copyright 2025 Jan de Cuveland
#pragma once

#include "ItemDistributor.hpp"
#include "Monitor.hpp"
#include "Parameters.hpp"
#include "Scheduler.hpp"
#include "TimesliceBufferFlex.hpp"
#include "TsBuilder.hpp"
#include <csignal>
#include <memory>
#include <zmq.hpp>

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

  zmq::context_t zmq_context_{1};

  std::unique_ptr<cbm::Monitor> monitor_;

  Scheduler tasks_;

  /// Address that is used for communication between the TimesliceBuffer and the
  /// ItemDistributor.
  const std::string producer_address_;

  /// Address that is used by Workers to connect to the ItemDistributor.
  const std::string worker_address_;

  /// The ItemDistributor object.
  ItemDistributor item_distributor_;

  /// Shared memory buffer to store received timeslices.
  TimesliceBufferFlex timeslice_buffer_;

  /// Thread for the ItemDistributor.
  std::thread distributor_thread_;

  /// TsBuilder instance
  std::unique_ptr<TsBuilder> ts_builder_;
};
