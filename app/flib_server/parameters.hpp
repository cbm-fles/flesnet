// Copyright 2015 Dirk Hutter

#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Utility.hpp"
#include "flib.h"
#include "log.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace po = boost::program_options;

class parameters {

public:
  parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  parameters(const parameters&) = delete;
  void operator=(const parameters&) = delete;

  size_t data_buffer_size_exp() { return _data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return _desc_buffer_size_exp; }

  std::string print_buffer_info() {
    std::stringstream ss;
    ss << "Buffer size per link: "
       << human_readable_count(UINT64_C(1) << _data_buffer_size_exp) << " + "
       << human_readable_count((UINT64_C(1) << _desc_buffer_size_exp) *
                               sizeof(fles::MicrosliceDescriptor));
    return ss.str();
  }

  bool flib_legacy_mode() const { return _flib_legacy_mode; }

private:
  void parse_options(int argc, char* argv[]) {

    std::string config_file;
    unsigned log_level;

    po::options_description generic("Generic options");
    generic.add_options()("help,h", "produce help message")(
        "config-file,c",
        po::value<std::string>(&config_file)->default_value("flib_server.cfg"),
        "name of a configuration file");

    po::options_description config(
        "Configuration (flib_server.cfg or cmd line)");
    config.add_options()

        ("data-buffer-size-exp",
         po::value<size_t>(&_data_buffer_size_exp)->default_value(27),
         "exp. size of the data buffer in bytes")(
            "desc-buffer-size-exp",
            po::value<size_t>(&_desc_buffer_size_exp)->default_value(19),
            "exp. size of the descriptor buffer (number of entries)")(
            "flib-legacy-mode", po::value<bool>(&_flib_legacy_mode),
            "use cbmnet flib")(
            "log-level,l", po::value<unsigned>(&log_level)->default_value(2),
            "set the log level (all:0)");

    po::options_description cmdline_options("Allowed options");
    cmdline_options.add(generic).add(config);

    po::options_description config_file_options;
    config_file_options.add(config);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    std::ifstream ifs(config_file.c_str());
    if (!ifs) {
      if (config_file != "flib_server.cfg") {
        std::cout << "Can not open config file: " << config_file << "\n";
        exit(EXIT_SUCCESS);
      }
    } else {
      std::cout << "Using config file: " << config_file << "\n";
      po::store(po::parse_config_file(ifs, config_file_options), vm);
      notify(vm);
    }

    if (vm.count("help")) {
      std::cout << cmdline_options << "\n";
      exit(EXIT_SUCCESS);
    }

    logging::add_console(static_cast<severity_level>(log_level));

    L_(info) << print_buffer_info();
  }

  size_t _data_buffer_size_exp;
  size_t _desc_buffer_size_exp;

  bool _flib_legacy_mode = true;
};
