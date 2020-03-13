// Copyright 2016 Farouk Salem <salem@zib.de>, Thorsten Schuett <schuett@zib.de>

#pragma once

#include "Scheduler.hpp"
#include "ThreadContainer.hpp"
#include "Utility.hpp"
#include "log.hpp"

class ConnectionGroupWorker : public ThreadContainer {

public:
  /// The "main" function of an IBConnectionGroup & ConnectionGroup decendant.
  virtual void operator()() = 0;
  virtual ~ConnectionGroupWorker() = default;
};
