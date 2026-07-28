// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "FlesnetPatternGenerator.hpp"
#include "MicrosliceAnalyzer.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "MicrosliceReceiver.hpp"
#include "Parameters.hpp"
#include "Sink.hpp" // MicrosliceSink
#include "TimesliceDebugger.hpp"
#include "log.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

Application::Application(Parameters const& par) : par_(par) {

  // Source setup
  if (par_.use_pattern_generator) {
    L_(info) << "using pattern generator as data source";

    constexpr uint32_t typical_content_size = 10000;
    constexpr std::size_t desc_buffer_size_exp = 19; // 512 ki entries
    constexpr std::size_t data_buffer_size_exp = 27; // 128 MiB

    data_source_ = std::make_unique<FlesnetPatternGenerator>(
        data_buffer_size_exp, desc_buffer_size_exp, par_.channel_idx,
        typical_content_size, true, true);
  }

  if (data_source_) {
    source_ = std::make_unique<fles::MicrosliceReceiver>(*data_source_);
  } else if (!par_.input_archive.empty()) {
    source_ =
        std::make_unique<fles::MicrosliceInputArchive>(par_.input_archive);
  }

  // Sink setup
  if (par_.analyze) {
    sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
        new MicrosliceAnalyzer(100000, 3, std::cout, "")));
  }

  if (par_.dump_verbosity > 0) {
    sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
        new MicrosliceDumper(std::cout, par_.dump_verbosity)));
  }

  if (!par_.output_archive.empty()) {
    sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
        new fles::MicrosliceOutputArchive(par_.output_archive)));
  }
}

Application::~Application() {
  L_(info) << "total microslices processed: " << count_;
}

void Application::run() {
  uint64_t limit = par_.maximum_number;

  while (auto microslice = source_->get()) {
    std::shared_ptr<const fles::Microslice> ms(std::move(microslice));
    for (auto& sink : sinks_) {
      sink->put(ms);
    }
    ++count_;
    if (count_ == limit) {
      break;
    }
  }
  for (auto& sink : sinks_) {
    sink->end_stream();
  }
}
