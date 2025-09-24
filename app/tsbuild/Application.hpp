/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include "ItemDistributor.hpp"
#include "Monitor.hpp"
#include "Parameters.hpp"
#include "TsBuffer.hpp"
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
  Parameters const& m_par;

  zmq::context_t m_zmq_context{1};

  std::unique_ptr<cbm::Monitor> m_monitor;

  /// Address that is used for communication between the TimesliceBuffer and the
  /// ItemDistributor.
  const std::string m_producer_address;

  /// Address that is used by Workers to connect to the ItemDistributor.
  const std::string m_worker_address;

  /// The ItemDistributor object.
  ItemDistributor m_item_distributor;

  /// Shared memory buffer to store received timeslices.
  TsBuffer m_timeslice_buffer;

  /// Thread for the ItemDistributor.
  std::thread m_distributor_thread;

  /// TsBuilder instance
  std::unique_ptr<TsBuilder> m_ts_builder;
};
