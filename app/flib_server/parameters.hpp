// Copyright 2015 Dirk Hutter

#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Utility.hpp"
#include "flib.h"
#include "log.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace po = boost::program_options;

struct pci_addr {
public:
  pci_addr(uint8_t bus = 0, uint8_t dev = 0, uint8_t func = 0)
      : bus(bus), dev(dev), func(func) {}
  uint8_t bus;
  uint8_t dev;
  uint8_t func;
};

// Overload validate for PCI BDF address
void validate(boost::any& v,
              const std::vector<std::string>& values,
              pci_addr*,
              int) {
  // PCI BDF address is BB:DD.F
  static boost::regex r("(\\d\\d):(\\d\\d).(\\d)");

  // Make sure no previous assignment to 'a' was made.
  po::validators::check_first_occurrence(v);
  // Extract the first string from 'values'. If there is more than
  // one string, it's an error, and exception will be thrown.
  const std::string& s = po::validators::get_single_string(values);

  // Do regex match and convert the interesting part.
  boost::smatch match;
  if (boost::regex_match(s, match, r)) {
    v = boost::any(pci_addr(boost::lexical_cast<unsigned>(match[1]),
                            boost::lexical_cast<unsigned>(match[2]),
                            boost::lexical_cast<unsigned>(match[3])));
  } else {
    throw po::validation_error(po::validation_error::invalid_option_value);
  }
}

class parameters {

public:
  parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  parameters(const parameters&) = delete;
  void operator=(const parameters&) = delete;

  bool flib_autodetect() const { return _flib_autodetect; }
  pci_addr flib_addr() const { return _flib_addr; }
  std::string shm() { return _shm; }
  std::string base_url() { return _base_url; }
  bool kv_sync() { return _kv_sync; }
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

        ("flib-addr,i", po::value<pci_addr>(),
         "PCI BDF address of target FLIB in BB:DD.F format")(
            "shm,o",
            po::value<std::string>(&_shm)->default_value("flib_shared_memory"),
            "name of the shared memory to be used")(
            "kv-sync", po::value<bool>(&_kv_sync),
            "use key-value store to synchronize with data source, bool")(
            "base-url",
            po::value<std::string>(&_base_url)
                ->default_value("http://localhost:2379/v2/keys/flesnet"),
            "url of key-value store")(
            "data-buffer-size-exp",
            po::value<size_t>(&_data_buffer_size_exp)->default_value(27),
            "exp. size of the data buffer in bytes")(
            "desc-buffer-size-exp",
            po::value<size_t>(&_desc_buffer_size_exp)->default_value(19),
            "exp. size of the descriptor buffer (number of entries)")(
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

    if (vm.count("flib-addr")) {
      _flib_addr = vm["flib-addr"].as<pci_addr>();
      _flib_autodetect = false;
      L_(debug) << "FLIB address: " << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(_flib_addr.bus) << ":" << std::setw(2)
                << std::setfill('0') << static_cast<unsigned>(_flib_addr.dev)
                << "." << static_cast<unsigned>(_flib_addr.func);
    } else {
      _flib_autodetect = true;
      L_(debug) << "FLIB address: autodetect";
    }

    L_(info) << "Shared menory file: " << _shm;
    L_(info) << print_buffer_info();
  }

  bool _flib_autodetect = true;
  pci_addr _flib_addr = {};
  std::string _shm;
  std::string _base_url;
  bool _kv_sync;
  size_t _data_buffer_size_exp;
  size_t _desc_buffer_size_exp;
};
