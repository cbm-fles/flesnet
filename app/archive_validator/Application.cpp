// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "FlesnetPatternGenerator.hpp"
#include "MicrosliceAnalyzer.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "MicrosliceReceiver.hpp"
#include "MicrosliceTransmitter.hpp"
#include "TimesliceDebugger.hpp"
#include "Verificator.hpp"
#include "log.hpp"
#include "shm_channel_client.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

Application::Application(Parameters const& par) : par_(par) {}

Application::~Application() {}

void Application::run() {
  if (par_.validate) {
    Verificator val;
    bool valid = val.verify_forward(par_.input_archives, par_.output_archives, par_.timeslice_cnt, par_.overlap);
    L_(info) << "Archive " << ((valid) ? "valid" : "NOT valid");
    return;
  }
}
