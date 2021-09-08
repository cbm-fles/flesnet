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

  /**
   * @brief Return a text description of the object (to be used as a thread
   * name).
   *
   * @return A string describing the object (at most 15 characters long).
   */
  [[nodiscard]] virtual std::string thread_name() const { return "CGWorker"; };

  virtual ~ConnectionGroupWorker() = default;
};
