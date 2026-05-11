#pragma once

#include "Parameters.hpp"

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
