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

/// Run parameters exception class.
class ParametersException : public std::runtime_error {
public:
  explicit ParametersException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

struct pci_addr {
public:
  pci_addr(uint8_t bus = 0, uint8_t dev = 0, uint8_t func = 0)
      : bus(bus), dev(dev), func(func) {}
  uint8_t bus;
  uint8_t dev;
  uint8_t func;
};

struct etcd_config_t {
  bool use_etcd = false;
  std::string authority;
  std::string path;
};

// Overload validate for PCI BDF address
void validate(boost::any& v,
              const std::vector<std::string>& values,
              pci_addr* /*unused*/,
              int /*unused*/) {
  // PCI BDF address is BB:DD.F
  static boost::regex r(
      "([[:xdigit:]][[:xdigit:]]):([[:xdigit:]][[:xdigit:]]).([[:xdigit:]])");

  // Make sure no previous assignment to 'a' was made.
  po::validators::check_first_occurrence(v);
  // Extract the first string from 'values'. If there is more than
  // one string, it's an error, and exception will be thrown.
  const std::string& s = po::validators::get_single_string(values);

  // Do regex match and convert the interesting part.
  boost::smatch match;
  if (boost::regex_match(s, match, r)) {
    v = boost::any(pci_addr(std::stoul(match[1], nullptr, 16),
                            std::stoul(match[2], nullptr, 16),
                            std::stoul(match[3], nullptr, 16)));
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
  size_t data_buffer_size_exp() { return _data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return _desc_buffer_size_exp; }
  etcd_config_t etcd() const { return _etcd; }
  std::string exec() const { return _exec; }

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
    unsigned log_level = 2;
    unsigned log_syslog = 2;
    std::string log_file;

    po::options_description generic("Generic options");
    auto generic_add = generic.add_options();
    generic_add("help,h", "produce help message");
    generic_add(
        "config-file,c",
        po::value<std::string>(&config_file)->default_value("flib_server.cfg"),
        "name of a configuration file");

    po::options_description config(
        "Configuration (flib_server.cfg or cmd line)");
    auto config_add = config.add_options();
    config_add("flib-addr,i", po::value<pci_addr>(),
               "PCI BDF address of target FLIB in BB:DD.F format");
    config_add(
        "shm,o",
        po::value<std::string>(&_shm)->default_value("flib_shared_memory"),
        "name of the shared memory to be used");
    config_add("data-buffer-size-exp",
               po::value<size_t>(&_data_buffer_size_exp)->default_value(27),
               "exp. size of the data buffer in bytes");
    config_add("desc-buffer-size-exp",
               po::value<size_t>(&_desc_buffer_size_exp)->default_value(19),
               "exp. size of the descriptor buffer (number of entries)");
    config_add("log-level,l",
               po::value<unsigned>(&log_level)
                   ->default_value(log_level)
                   ->value_name("<n>"),
               "set the file log level (all:0)");
    config_add("log-file,L", po::value<std::string>(&log_file),
               "name of target log file");
    config_add("log-syslog,S",
               po::value<unsigned>(&log_syslog)
                   ->default_value(log_syslog)
                   ->value_name("<n>"),
               "enable logging to syslog at given log level");
    config_add("etcd-authority",
               po::value<std::string>(&_etcd.authority)
                   ->default_value("127.0.0.1:2379"),
               "where to find the etcd server");
    config_add("etcd-path", po::value<std::string>(&_etcd.path),
               "base path for this instance, leave empty to not use etcd");
    config_add("exec,e", po::value<std::string>(&_exec)->value_name("<string>"),
               "name of an executable to run after startup");

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
        throw ParametersException("Cannot open config file: " + config_file);
      }
    } else {
      std::cout << "Using config file: " << config_file << "\n";
      po::store(po::parse_config_file(ifs, config_file_options), vm);
      notify(vm);
    }

    if (vm.count("help") != 0u) {
      std::cout << cmdline_options << "\n";
      exit(EXIT_SUCCESS);
    }

    logging::add_console(static_cast<severity_level>(log_level));
    if (vm.count("log-file") != 0u) {
      L_(info) << "Logging output to " << log_file;
      logging::add_file(log_file, static_cast<severity_level>(log_level));
    }

    if (vm.count("log-syslog") != 0u) {
      logging::add_syslog(logging::syslog::local0,
                          static_cast<severity_level>(log_syslog));
    }

    if (vm.count("flib-addr") != 0u) {
      _flib_addr = vm["flib-addr"].as<pci_addr>();
      _flib_autodetect = false;
      L_(debug) << "FLIB address: " << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<unsigned>(_flib_addr.bus)
                << ":" << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(_flib_addr.dev) << "."
                << static_cast<unsigned>(_flib_addr.func);
    } else {
      _flib_autodetect = true;
      L_(debug) << "FLIB address: autodetect";
    }

    if (vm.count("etcd-path") != 0u) {
      _etcd.use_etcd = true;
    }

    L_(info) << "Shared memory file: " << _shm;
    L_(info) << print_buffer_info();
  }

  bool _flib_autodetect = true;
  pci_addr _flib_addr = {};
  std::string _shm;
  size_t _data_buffer_size_exp;
  size_t _desc_buffer_size_exp;
  etcd_config_t _etcd;
  std::string _exec;
};
