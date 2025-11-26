/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Jan de Cuveland, Dirk Hutter */
#pragma once

#include "Monitor.hpp"
#include "Parameters.hpp"
#include "StBuilder.hpp"
#include "StSender.hpp"
#include <csignal>
#include <memory>

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

  std::unique_ptr<cbm::Monitor> m_monitor;

  std::unique_ptr<StSender> m_st_sender;
  std::unique_ptr<StBuilder> m_st_builder;
};
