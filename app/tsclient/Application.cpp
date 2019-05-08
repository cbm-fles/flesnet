// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "TimesliceAnalyzer.hpp"
#include "TimesliceDebugger.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimeslicePublisher.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceSubscriber.hpp"
#include "Utility.hpp"
#include <boost/lexical_cast.hpp>
#include <thread>

Application::Application(Parameters const& par) : par_(par) {
  if (!par_.shm_identifier().empty()) {
    source_.reset(new fles::TimesliceReceiver(par_.shm_identifier()));
  } else if (!par_.input_archive().empty()) {
    if (par_.input_archive_cycles() <= 1) {
      source_.reset(new fles::TimesliceInputArchive(par_.input_archive()));
    } else {
      source_.reset(new fles::TimesliceInputArchiveLoop(
          par_.input_archive(), par_.input_archive_cycles()));
    }
  } else if (!par_.subscribe_address().empty()) {
    source_.reset(new fles::TimesliceSubscriber(par_.subscribe_address(),
                                                par_.subscribe_hwm()));
  }

  if (par_.analyze()) {
    std::string output_prefix =
        boost::lexical_cast<std::string>(par_.client_index()) + ": ";
    sinks_.push_back(std::unique_ptr<fles::TimesliceSink>(
        new TimesliceAnalyzer(1000, status_log_.stream, output_prefix)));
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
    benchmark_.reset(new Benchmark());
  }

  if (par_.client_index() != -1) {
    L_(info) << "tsclient " << par_.client_index() << ": "
             << par.shm_identifier();
  }

  if (par_.rate_limit() != 0.0) {
    L_(info) << "rate limit active: "
             << human_readable_count(par_.rate_limit(), true, "Hz");
  }
}

Application::~Application() {
  if (par_.client_index() != -1) {
    L_(info) << "tsclient " << par_.client_index() << ": ";
  }
  L_(info) << "total timeslices processed: " << count_;
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

  while (auto timeslice = source_->get()) {
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
