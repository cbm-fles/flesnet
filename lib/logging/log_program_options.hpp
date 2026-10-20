// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief The same logging command line options for every application.
///
/// Uses Boost.Program_options, which libfles_logging itself does not depend
/// on; applications including this header link it anyway.
#pragma once

#include "log.hpp"

#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>

#include <stdexcept>
#include <string>

namespace logging {

/// Adds -l/--log-level, --log-format, --log-file and --log-file-level.
inline void
add_program_options(boost::program_options::options_description& desc) {
  namespace po = boost::program_options;
  auto add = desc.add_options();
  add("log-level,l", po::value<std::string>()->value_name("LEVEL"),
      "log level on stderr: trace, debug, status, info, warning, error, "
      "fatal or 0-6 (default: status, or CBM_LOG_LEVEL)");
  add("log-format", po::value<std::string>()->value_name("FORMAT"),
      "log format on stderr: text or json (default: text, or "
      "CBM_LOG_FORMAT). JSON names the levels status, info and warning "
      "INFO, NOTICE and WARN");
  add("log-file", po::value<std::string>()->value_name("FILENAME"),
      "also append the log to FILENAME as JSON lines (or CBM_LOG_FILE)");
  add("log-file-level", po::value<std::string>()->value_name("LEVEL"),
      "log level for --log-file (default: the stderr level, or "
      "CBM_LOG_FILE_LEVEL)");
}

/// The logging options from the command line, where given, on top of the
/// given options (by default those from the environment).
inline Options options_from(boost::program_options::variables_map const& vm,
                            Options options = options_from_environment()) {
  auto apply = [&vm](char const* name, auto const& set) {
    if (vm.count(name) == 0) {
      return;
    }
    auto const& value = vm[name].as<std::string>();
    try {
      set(value);
    } catch (std::invalid_argument const& e) {
      throw boost::program_options::error(std::string("--") + name + ": " +
                                          e.what());
    }
  };
  apply("log-level",
        [&](std::string const& v) { options.level = parse_level(v); });
  apply("log-format",
        [&](std::string const& v) { options.format = parse_format(v); });
  apply("log-file", [&](std::string const& v) { options.file = v; });
  apply("log-file-level",
        [&](std::string const& v) { options.file_level = parse_level(v); });
  return options;
}

} // namespace logging
