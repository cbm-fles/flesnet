// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "TimesliceAnalyzer.hpp"
#include "TimesliceAutoSource.hpp"
#include "TimesliceDebugger.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimeslicePublisher.hpp"
#include "Utility.hpp"
#include <memory>
#include <thread>

Application::Application(Parameters const& par) : par_(par) {
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

  if (!par_.output_archive().empty()) {
    if (par_.output_archive_items() == SIZE_MAX &&
        par_.output_archive_bytes() == SIZE_MAX) {
      sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
          new fles::TimesliceOutputArchive(par_.output_archive())));
    } else {
      sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
          new fles::TimesliceOutputArchiveSequence(
              par_.output_archive(), par_.output_archive_items(),
              par_.output_archive_bytes())));
    }
  }

  if (!par_.publish_address().empty()) {
    sinks_.push_back(
        std::unique_ptr<fles::TimesliceSink>(new fles::TimeslicePublisher(
            par_.publish_address(), par_.publish_hwm())));
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
    if (par_.rate_limit() != 0.0) {
      rate_limit_delay();
    }
    for (auto& sink : sinks_) {
      sink->put(ts);
    }
    ++count_;
    if (count_ == limit) {
      break;
    }
  }
}
