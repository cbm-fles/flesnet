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
#include <iostream>
#include <memory>
#include <thread>

Application::Application(Parameters const& par) : par_(par) {

  // Source setup
  if (!par_.input_shm.empty()) {
    L_(info) << "using shared memory as data source: " << par_.input_shm;

    shm_device_ = std::make_shared<flib_shm_device_client>(par_.input_shm);

    if (par_.channel_idx < shm_device_->num_channels()) {
      data_source_ = std::make_unique<flib_shm_channel_client>(
          shm_device_, par_.channel_idx);

    } else {
      throw std::runtime_error("shared memory channel not available");
    }
  } else if (par_.use_pattern_generator) {
    L_(info) << "using pattern generator as data source";

    constexpr uint32_t typical_content_size = 10000;
    constexpr std::size_t desc_buffer_size_exp = 19; // 512 ki entries
    constexpr std::size_t data_buffer_size_exp = 27; // 128 MiB

    data_source_ = std::make_unique<FlesnetPatternGenerator>(
        data_buffer_size_exp, desc_buffer_size_exp, par_.channel_idx,
        typical_content_size, true, false);
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
        new MicrosliceAnalyzer(100000, 3, std::cout, "", par_.channel_idx)));
  }

  if (par_.dump_verbosity > 0) {
    sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
        new MicrosliceDumper(std::cout, par_.dump_verbosity)));
  }

  if (!par_.output_archive.empty()) {
    sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
        new fles::MicrosliceOutputArchive(par_.output_archive)));
  }

  if (!par_.output_shm.empty()) {
    L_(info) << "providing output in shared memory: " << par_.output_shm;

    constexpr std::size_t desc_buffer_size_exp = 19; // 512 ki entries
    constexpr std::size_t data_buffer_size_exp = 27; // 128 MiB

    output_shm_device_ = std::make_unique<flib_shm_device_provider>(
        par_.output_shm, 1, data_buffer_size_exp, desc_buffer_size_exp);
    InputBufferWriteInterface* data_sink = output_shm_device_->channels().at(0);
    sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
        new fles::MicrosliceTransmitter(*data_sink)));
  }
}

Application::~Application() {
  L_(info) << "total microslices processed: " << count_;
}

void Application::run() {
  uint64_t limit = par_.maximum_number;

  if (par_.validate_) {
    Verificator val;
    bool valid = val.verify(par_.input_archives_, par_.output_archives_, 100, 10);
    std::cout << "valid: " << valid << std::endl;
    return;
  }

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
  if (output_shm_device_) {
    L_(info) << "waiting until output shared memory is empty";
    while (!output_shm_device_->channels().at(0)->empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}
