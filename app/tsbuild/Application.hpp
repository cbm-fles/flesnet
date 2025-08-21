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

  /*
  size_t timeslice_count_ = 0;  ///< total number of processed timeslices
  size_t component_count_ = 0;  ///< total number of processed components
  size_t microslice_count_ = 0; ///< total number of processed microslices
  size_t content_bytes_ = 0;    ///< total number of processed content bytes
  size_t total_bytes_ = 0;      ///< total number of processed bytes
  size_t timeslice_incomplete_count_ = 0; ///< number of incomplete timeslices
*/

  void report_status();

  std::unique_ptr<cbm::Monitor> monitor_;
  std::string hostname_;

  Scheduler scheduler_;

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
