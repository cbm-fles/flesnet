// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "ManagedTimesliceBuffer.hpp"
#include "Timeslice.hpp"
#include "TimesliceAnalyzer.hpp"
#include "TimesliceAutoSource.hpp"
#include "TimesliceDebugger.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimeslicePublisher.hpp"
#include "Utility.hpp"
#include <thread>
#include <utility>

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status) {
  // start up monitoring
  if (!par.monitor_uri().empty()) {
    monitor_ = std::make_unique<cbm::Monitor>(par_.monitor_uri());
  }

  if (par_.client_index() != -1) {
    output_prefix_ = std::to_string(par_.client_index()) + ": ";
  }

  source_ = std::make_unique<fles::TimesliceAutoSource>(par_.input_uri());

  if (par_.analyze()) {
    if (par_.histograms()) {
      sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
          new TimesliceAnalyzer(1000, status_log_.stream, output_prefix_,
                                &std::cout, monitor_.get())));
    } else {
      sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
          new TimesliceAnalyzer(1000, status_log_.stream, output_prefix_,
                                nullptr, monitor_.get())));
    }
  }

  if (par_.verbosity() > 0) {
    sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
        new TimesliceDumper(debug_log_.stream, par_.verbosity())));
  }

  bool has_shm_output = false;
  for (const auto& output_uri : par_.output_uris()) {
    // If output_uri has no full URI pattern, everything is in "uri.path"
    UriComponents uri{output_uri};

    if (uri.scheme == "file" || uri.scheme.empty()) {
      size_t items = SIZE_MAX;
      size_t bytes = SIZE_MAX;
      fles::ArchiveCompression compression = fles::ArchiveCompression::None;
      for (auto& [key, value] : uri.query_components) {
        if (key == "items") {
          items = stoull(value);
        } else if (key == "bytes") {
          bytes = stoull(value);
        } else if (key == "c") {
          if (value == "none") {
            compression = fles::ArchiveCompression::None;
          } else if (value == "zstd") {
            compression = fles::ArchiveCompression::Zstd;
          } else {
            throw std::runtime_error(
                "invalid compression type for scheme file: " + value);
          }
        } else {
          throw std::runtime_error(
              "query parameter not implemented for scheme file: " + key);
        }
      }
      const auto file_path = uri.authority + uri.path;
      if (items == SIZE_MAX && bytes == SIZE_MAX) {
        sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
            new fles::TimesliceOutputArchive(file_path, compression)));
      } else {
        sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
            new fles::TimesliceOutputArchiveSequence(file_path, items, bytes,
                                                     compression)));
      }

    } else if (uri.scheme == "tcp") {
      uint32_t hwm = 1;
      for (auto& [key, value] : uri.query_components) {
        if (key == "hwm") {
          hwm = stou(value);
        } else {
          throw std::runtime_error(
              "query parameter not implemented for scheme " + uri.scheme +
              ": " + key);
        }
      }
      const auto address = uri.scheme + "://" + uri.authority;
      sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
          new fles::TimeslicePublisher(address, hwm)));

    } else if (uri.scheme == "shm") {
      uint32_t num_components = 1;
      uint32_t datasize = 27; // 128 MiB
      uint32_t descsize = 19; // 16 MiB
      for (auto& [key, value] : uri.query_components) {
        if (key == "datasize") {
          datasize = std::stoul(value);
        } else if (key == "descsize") {
          descsize = std::stoul(value);
        } else if (key == "n") {
          num_components = std::stoul(value);
        } else {
          throw std::runtime_error(
              "query parameter not implemented for scheme " + uri.scheme +
              ": " + key);
        }
      }
      const auto shm_identifier = split(uri.path, "/").at(0);
      sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
          new ManagedTimesliceBuffer(zmq_context_, shm_identifier, datasize,
                                     descsize, num_components)));
      has_shm_output = true;

    } else {
      throw ParametersException("invalid output scheme: " + uri.scheme);
    }
  }

  if (has_shm_output) {
    // wait a moment to allow the ManagedTimesliceBuffer clients to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (par_.benchmark()) {
    benchmark_ = std::make_unique<Benchmark>();
  }

  if (par_.rate_limit() != 0.0) {
    L_(info) << output_prefix_ << "rate limit active: "
             << human_readable_count(par_.rate_limit(), true, "Hz");
  }
}

Application::~Application() {
  L_(info) << output_prefix_ << "total timeslices processed: " << count_;

  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = std::chrono::milliseconds(200);
  std::this_thread::sleep_for(destruct_delay);
}

void Application::rate_limit_delay() const {
  auto delta_is = std::chrono::high_resolution_clock::now() - time_begin_;
  auto delta_want = std::chrono::microseconds(
      static_cast<uint64_t>(count_ * 1.0e6 / par_.rate_limit()));

  if (delta_want > delta_is) {
    std::this_thread::sleep_for(delta_want - delta_is);
  }
}

void Application::native_speed_delay(uint64_t ts_start_time) {
  if (count_ == 0) {
    first_ts_start_time_ = ts_start_time;
  } else {
    auto delta_is = std::chrono::high_resolution_clock::now() - time_begin_;
    auto delta_want =
        std::chrono::nanoseconds(ts_start_time - first_ts_start_time_) /
        par_.native_speed();
    if (delta_want > delta_is) {
      std::this_thread::sleep_for(delta_want - delta_is);
    }
  }
}

void Application::run() {
  time_begin_ = std::chrono::high_resolution_clock::now();

  if (benchmark_) {
    benchmark_->run();
    return;
  }

  uint64_t limit = par_.maximum_number();

  uint64_t index = 0;
  while (auto timeslice = source_->get()) {
    if (index >= par_.offset() &&
        (index - par_.offset()) % par_.stride() == 0) {
      ++index;
    } else {
      ++index;
      continue;
    }
    std::shared_ptr<const fles::Timeslice> ts(std::move(timeslice));
    if (par_.native_speed() != 0.0) {
      native_speed_delay(ts->start_time());
    }
    if (par_.rate_limit() != 0.0) {
      rate_limit_delay();
    }
    for (auto& sink : sinks_) {
      sink->put(ts);
    }
    ++count_;
    if (count_ == limit || *signal_status_ != 0) {
      break;
    }
    // avoid unneccessary pipelining
    timeslice.reset();
  }

  // Loop over sinks. For all sinks of type ManagedTimesliceBuffer, check if
  // they are empty. If at least one of them is not empty, wait for 100 ms.
  // Repeat until all sinks are empty.
  bool all_empty = false;
  bool first = true;
  *signal_status_ = 0;
  while (!all_empty && *signal_status_ == 0) {
    all_empty = true;
    for (auto& sink : sinks_) {
      auto* mtb = dynamic_cast<ManagedTimesliceBuffer*>(sink.get());
      if (mtb != nullptr) {
        mtb->handle_timeslice_completions();
        if (!mtb->empty()) {
          all_empty = false;
          if (first) {
            L_(info) << output_prefix_ << "waiting for shm buffer to empty";
            L_(info) << output_prefix_ << "press Ctrl-C to abort";
            first = false;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          break;
        }
      }
    }
  }
}
