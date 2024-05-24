// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DualRingBuffer.hpp"
#include "MicrosliceSource.hpp"
#include "Parameters.hpp"
#include "Sink.hpp"
#include "shm_device_client.hpp"
#include "shm_device_provider.hpp"
#include <memory>
#include <vector>

/// %Application base class.
class Application {
public:
  explicit Application(Parameters const& par);

  Application(const Application&) = delete;
  void operator=(const Application&) = delete;

  ~Application();

  void run();

private:
  Parameters const& par_;
};
