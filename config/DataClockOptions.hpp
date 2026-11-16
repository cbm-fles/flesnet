// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief The data clock options shared by the processes of a timeslice
/// building setup.
#pragma once

#include "System.hpp"
#include "log.hpp"
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <stdexcept>
#include <string>

namespace fles {

/// Adds the options that configure the data clock (see
/// fles::system::DataClock). Every process of a timeslice building setup has
/// to be given the same values.
inline void
add_data_clock_options(boost::program_options::options_description& desc) {
  namespace po = boost::program_options;
  auto add = desc.add_options();
  add("data-time-origin", po::value<std::string>()->value_name("TIME"),
      "data time that the data clock reads at the reference instant, as "
      "nanoseconds or ISO 8601 (default: the reference instant itself, so "
      "that the data clock is the system clock)");
  add("data-time-reference", po::value<std::string>()->value_name("TIME"),
      "system time at which the data clock reads the data time origin, as "
      "nanoseconds since the Unix epoch or ISO 8601 (default: the Unix "
      "epoch)");
  add("data-clock-rate", po::value<double>()->value_name("X"),
      "speed of the data clock relative to the system clock (default: 1)");
}

/// The data clock described by the options.
inline fles::system::DataClock
data_clock_options(boost::program_options::variables_map const& vm) {
  fles::system::DataClock clock;
  if (vm.count("data-time-reference") != 0u) {
    clock.real_origin_ns = fles::system::parse_time_point_ns(
        vm["data-time-reference"].as<std::string>());
  }
  clock.data_origin_ns = clock.real_origin_ns;
  if (vm.count("data-time-origin") != 0u) {
    clock.data_origin_ns = fles::system::parse_time_point_ns(
        vm["data-time-origin"].as<std::string>());
  }
  if (vm.count("data-clock-rate") != 0u) {
    clock.rate = vm["data-clock-rate"].as<double>();
    if (!(clock.rate > 0.0)) {
      throw std::invalid_argument("data clock rate must be positive");
    }
  }
  return clock;
}

/// Sets the data clock of this process from the options and reports a clock
/// other than the system clock. Call it after the logging setup.
inline void setup_data_clock(boost::program_options::variables_map const& vm) {
  auto clock = data_clock_options(vm);
  fles::system::set_data_clock(clock);
  if (!clock.is_system_clock()) {
    LF(info, "Data clock: {}", clock.to_string());
  }
}

} // namespace fles
