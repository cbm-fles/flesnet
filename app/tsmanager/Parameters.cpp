/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "Parameters.hpp"
#include "CommonOptions.hpp"
#include "System.hpp"
#include "log.hpp"
#include "log_program_options.hpp"
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[]) {

  std::string config_file;

  auto terminal_width = fles::system::current_terminal_width();

  po::options_description generic("Generic options", terminal_width,
                                  terminal_width / 2);
  auto generic_add = generic.add_options();
  fles::add_general_options(generic);
  generic_add(
      "config-file,c",
      po::value<std::string>(&config_file)->default_value("tsmanager.cfg"),
      "name of a configuration file");

  po::options_description config("Configuration (tsmanager.cfg or cmd line)",
                                 terminal_width, terminal_width / 2);
  auto config_add = config.add_options();
  logging::add_program_options(config);
  config_add("monitor,m",
             po::value<std::string>(&m_monitor_uri)
                 ->value_name("URI")
                 ->implicit_value("influx1:login:8086:flesnet_status"),
             "publish tsmanager status to InfluxDB (or \"file:cout\" for "
             "console output)");
  config_add("listen-port,p",
             po::value<uint16_t>(&m_listen_port)->default_value(m_listen_port),
             "port to listen for stsender and tsbuilder connections");
  config_add("timeslice-duration",
             po::value<Nanoseconds>(&m_timeslice_duration)
                 ->default_value(m_timeslice_duration),
             "duration of a timeslice (with suffix ns, us, ms, s)");
  config_add("timeout",
             po::value<Nanoseconds>(&m_timeout)->default_value(m_timeout),
             "timeout for data reception (with suffix ns, us, ms, s)");
  config_add(
      "max-in-flight",
      po::value<uint32_t>(&m_max_in_flight)->default_value(m_max_in_flight),
      "maximum number of timeslices in flight per builder");

  po::options_description cmdline_options("Allowed options", terminal_width,
                                          terminal_width / 2);
  cmdline_options.add(generic).add(config);

  po::options_description config_file_options;
  config_file_options.add(config);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);

  fles::handle_general_options(vm, "tsmanager", cmdline_options);

  std::ifstream ifs(config_file.c_str());
  if (!ifs) {
    if (config_file != "tsmanager.cfg") {
      throw ParametersException("Cannot open config file: " + config_file);
    }
  } else {
    std::cout << "Using config file: " << config_file << "\n";
    po::store(po::parse_config_file(ifs, config_file_options), vm);
    notify(vm);
  }

  logging::setup(fles::log_options(vm));

  if (timeslice_duration_ns() <= 0) {
    throw ParametersException("timeslice duration must be greater than 0");
  }
  if (timeout_ns() <= 0) {
    throw ParametersException("timeout must be greater than 0");
  }
  if (m_max_in_flight == 0) {
    throw ParametersException("max-in-flight must be greater than 0");
  }
}
