/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include "Monitor.hpp"
#include "Parameters.hpp"
#include "TimesliceShmBuffer.hpp"
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

  /// Shared memory buffer to store received timeslices.
  fles::TimesliceShmBuffer m_timeslice_buffer;

  /// TsBuilder instance
  std::unique_ptr<TsBuilder> m_ts_builder;
};
