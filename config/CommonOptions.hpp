// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief The options that all flesnet applications share.
#pragma once

#include "GitRevision.hpp"
#include "log.hpp"
#include "log_program_options.hpp"
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace fles {

/// Adds -h/--help and -V/--version.
inline void
add_general_options(boost::program_options::options_description& desc) {
  auto add = desc.add_options();
  add("help,h", "produce help message");
  add("version,V", "print version string");
}

/// Prints the version, for --help followed by the options, and exits if one
/// of the two is given. Call it right after parsing the command line.
inline void handle_general_options(
    boost::program_options::variables_map const& vm,
    std::string_view program,
    boost::program_options::options_description const& options) {
  bool const help = vm.count("help") != 0u;
  if (!help && vm.count("version") == 0u) {
    return;
  }
  std::cout << program << " " << g_PROJECT_VERSION_GIT << ", git revision "
            << g_GIT_REVISION << "\n";
  if (help) {
    std::cout << options << "\n";
  }
  std::exit(EXIT_SUCCESS);
}

/// The logging options from the command line and the environment. Every JSON
/// line gets the software version, which includes the git commit, as
/// "version".
inline logging::Options
log_options(boost::program_options::variables_map const& vm) {
  auto options = logging::options_from(vm);
  options.context.push_back(logging::field("version", g_PROJECT_VERSION_GIT));
  return options;
}

} // namespace fles
