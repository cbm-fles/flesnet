// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComponentBuilder.hpp"
#include "ItemDistributor.hpp"
#include "ItemProducer.hpp"
#include "Monitor.hpp"
#include "Parameters.hpp"
#include "cri_device.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <csignal>
#include <memory>
#include <vector>
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
  volatile sig_atomic_t* signal_status_;

  /// The application's monitoring object
  std::unique_ptr<cbm::Monitor> monitor_;

  /// The application's ZeroMQ context
  zmq::context_t zmq_context_{1};

  std::vector<std::unique_ptr<cri::cri_device>> cris_;
  std::vector<cri::cri_channel*> cri_channels_;
  std::unique_ptr<boost::interprocess::managed_shared_memory> shm_;
  std::vector<std::unique_ptr<ComponentBuilder>> builders_;

  std::unique_ptr<ItemProducer> item_producer_;
  std::unique_ptr<ItemDistributor> item_distributor_;
};
