// Copyright 2025 Jan de Cuveland
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
  volatile std::sig_atomic_t* m_signalStatus;

  zmq::context_t m_zmqContext{1};

  std::unique_ptr<cbm::Monitor> m_monitor;

  /// Address that is used for communication between the TimesliceBuffer and the
  /// ItemDistributor.
  const std::string m_producerAddress;

  /// Address that is used by Workers to connect to the ItemDistributor.
  const std::string m_workerAddress;

  /// The ItemDistributor object.
  ItemDistributor m_itemDistributor;

  /// Shared memory buffer to store received timeslices.
  TsBuffer m_timesliceBuffer;

  /// Thread for the ItemDistributor.
  std::thread m_distributorThread;

  /// TsBuilder instance
  std::unique_ptr<TsBuilder> m_tsBuilder;
};
