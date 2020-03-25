// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Pierre-Alain Loizeau <p.-a.loizeau@gsi.de>

#include "Application.hpp"
#include "NgdpbDebugger.hpp"
//#include "MicrosliceAnalyzer.hpp"
#include "MicrosliceInputArchive.hpp"
//#include "MicrosliceOutputArchive.hpp"
#include "MicrosliceReceiver.hpp"
//#include "MicrosliceTransmitter.hpp"
//#include "TimesliceDebugger.hpp"
#include "log.hpp"
#include "shm_channel_client.hpp"
#include <chrono>
#include <iostream>
#include <thread>

Application::Application(Parameters const& par) : par_(par) {

  // Source setup
  if (!par_.input_shm.empty()) {
    L_(info) << "using shared memory as data source: " << par_.input_shm;

    shm_device_ = std::make_shared<flib_shm_device_client>(par_.input_shm);

    if (par_.shm_channel < shm_device_->num_channels()) {
      data_source_.reset(
          new flib_shm_channel_client(shm_device_, par_.shm_channel));

    } else {
      throw std::runtime_error("shared memory channel not available");
    }
  }

  if (data_source_) {
    source_.reset(new fles::MicrosliceReceiver(*data_source_));
  } else if (!par_.input_archive.empty()) {
    source_.reset(new fles::MicrosliceInputArchive(par_.input_archive));
  }

  // Sink setup
  if (par_.debugger) {
    sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
        new NgdpbMicrosliceDumper(std::cout, par_.dump_verbosity)));
  }

  if (!par_.output_archive.empty()) {
    if (par_.sorter) {
      filtered_output_archive_ =
          new fles::MicrosliceOutputArchive(par_.output_archive);
      ngdpb_sorter_arch_ =
          new fles::NdpbEpochToMsSorter(par_.epoch_per_ms, par_.sortmesg);
      sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
          new fles::FilteringMicrosliceSink(*filtered_output_archive_,
                                            *ngdpb_sorter_arch_)));
    } else {
      sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
          new fles::MicrosliceOutputArchive(par_.output_archive)));
    }
  }

  if (!par_.output_shm.empty()) {
    L_(info) << "providing output in shared memory: " << par_.output_shm;

    constexpr std::size_t desc_buffer_size_exp = 19; // 512 ki entries
    constexpr std::size_t data_buffer_size_exp = 27; // 128 MiB

    output_shm_device_.reset(new flib_shm_device_provider(
        par_.output_shm, 1, data_buffer_size_exp, desc_buffer_size_exp));
    InputBufferWriteInterface* data_sink = output_shm_device_->channels().at(0);
    if (par_.sorter) {
      filtered_output_mstrans_ = new fles::MicrosliceTransmitter(*data_sink);
      ngdpb_sorter_shm_ =
          new fles::NdpbEpochToMsSorter(par_.epoch_per_ms, par_.sortmesg);
      sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
          new fles::FilteringMicrosliceSink(*filtered_output_mstrans_,
                                            *ngdpb_sorter_shm_)));
    } else {
      sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
          new fles::MicrosliceTransmitter(*data_sink)));
    }
  }
  tStart = std::chrono::steady_clock::now();
}

Application::~Application() {
  std::chrono::steady_clock::time_point tStop =
      std::chrono::steady_clock::now();
  std::chrono::duration<double> time_span =
      std::chrono::duration_cast<std::chrono::duration<double>>(tStop - tStart);

  L_(info) << "total microslices processed: " << count_;
  L_(info) << "processing time: " << time_span.count() << " seconds";

  // cleanup memory
  delete filtered_output_archive_;
  delete ngdpb_sorter_arch_;
  delete filtered_output_mstrans_;
  delete ngdpb_sorter_shm_;
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
  if (output_shm_device_) {
    L_(info) << "waiting until output shared memory is empty";
    while (!output_shm_device_->channels().at(0)->empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}
