/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "cri.hpp"
#include "log.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace po = boost::program_options;

static const size_t _num_max_channels = 8;

typedef enum { flim, pgen, disable } data_source;

/// Run parameters exception class.
class ParametersException : public std::runtime_error {
public:
  explicit ParametersException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

struct channel_config {
  data_source source;
  uint16_t eq_id;
};

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

  bool dev_autodetect() const { return _dev_autodetect; }
  pci_addr dev_addr() const { return _pci_addr; }
  bool identify() const { return _identify; }
  uint32_t mc_size() const { return _mc_size; }
  float pgen_rate() const { return _pgen_rate; }
  uint32_t mc_size_limit() const { return _mc_size_limit; }

  struct channel_config channel(size_t i) const {
    return _channels.at(i);
  }

private:
  void parse_eq_id(const po::variables_map& vm, size_t i) {
    if (vm.count("c" + std::to_string(i) + "_eq_id") != 0u) {
      _channels.at(i).eq_id = boost::numeric_cast<uint16_t>(
          std::stoul(vm["c" + std::to_string(i) + "_eq_id"].as<std::string>(),
                     nullptr, 0));
    } else {
      throw ParametersException("If reading from pgen eq_id is required.");
    }
  }

  void parse_data_source(const po::variables_map& vm, size_t i) {
    if (vm.count("c" + std::to_string(i) + "_source") !=
        0u) { // set given parameters
      std::string source =
          vm["c" + std::to_string(i) + "_source"].as<std::string>();
      if (source == "flim") {
        _channels.at(i).source = flim;
        L_(info) << " data source: flim";
      } else if (source == "pgen") {
        _channels.at(i).source = pgen;
        parse_eq_id(vm, i);
        L_(info) << " data source: pgen (0x" << std::hex
                 << _channels.at(i).eq_id << ")";
      } else if (source == "disable") {
        _channels.at(i).source = disable;
        L_(info) << " data source: disable";
      } else {
        throw ParametersException("No valid arg for data source.");
      }
    } else { // set default parameters
      _channels.at(i).source = disable;
      L_(info) << " data source: disable (default)";
    }
  }

  void parse_options(int argc, char* argv[]) {

    std::string config_file;
    unsigned log_level;
    std::string log_file;

    po::options_description generic("Generic options");
    auto generic_add = generic.add_options();
    generic_add("help,h", "produce help message");
    generic_add("config-file,c",
                po::value<std::string>(&config_file)->implicit_value("cri.cfg"),
                "name of a configuration file");
    generic_add("log-level,l",
                po::value<unsigned>(&log_level)->default_value(2),
                "set the log level (all:0)");
    generic_add("log-file,L", po::value<std::string>(&log_file),
                "name of target log file");

    po::options_description config("Configuration (cri.cfg or cmd line)");
    auto config_add = config.add_options();
    config_add("pci-addr,i", po::value<pci_addr>(),
               "PCI BDF address of target CRI in BB:DD.F format");
    config_add("identify", po::value<bool>(&_identify)->default_value(false),
               "toggle CRI ID led");
    config_add("mc-size,t", po::value<uint32_t>(),
               "size of pattern generator microslices in units of "
               "1 us (31 bit wide)");
    config_add("pgen-rate,r", po::value<float>(),
               "MS fill level of pattern generator in [0,1]");
    config_add("mc-size-limit", po::value<uint32_t>(&_mc_size_limit),
               "Threshold of microslice size limiter in bytes.");

    config_add("c0_source", po::value<std::string>(),
               "Channel 0 data source <disable|flim|pgen>");
    config_add("c0_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 0 pgen data source (16 Bit)");
    config_add("c1_source", po::value<std::string>(),
               "Channel 1 data source <disable|flim|pgen>");
    config_add("c1_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 1 pgen data source (16 Bit)");
    config_add("c2_source", po::value<std::string>(),
               "Channel 2 data source <disable|flim|pgen>");
    config_add("c2_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 2 pgen data source (16 Bit)");
    config_add("c3_source", po::value<std::string>(),
               "Channel 3 data source <disable|flim|pgen>");
    config_add("c3_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 3 pgen data source (16 Bit)");
    config_add("c4_source", po::value<std::string>(),
               "Channel 4 data source <disable|flim|pgen>");
    config_add("c4_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 4 pgen data source (16 Bit)");
    config_add("c5_source", po::value<std::string>(),
               "Channel 5 data source <disable|flim|pgen>");
    config_add("c5_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 5 pgen data source (16 Bit)");
    config_add("c6_source", po::value<std::string>(),
               "Channel 6 data source <disable|flim|pgen>");
    config_add("c6_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 6 pgen data source (16 Bit)");
    config_add("c7_source", po::value<std::string>(),
               "Channel 7 data source <disable|flim|pgen>");
    config_add("c7_eq_id", po::value<std::string>(),
               "Equipment identifier of channel 7 pgen data source (16 Bit)");

    po::options_description cmdline_options("Allowed options");
    cmdline_options.add(generic).add(config);

    po::options_description config_file_options;
    config_file_options.add(config);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    logging::add_console(static_cast<severity_level>(log_level));
    if (vm.count("log-file") != 0u) {
      L_(info) << "Logging output to " << log_file;
      logging::add_file(log_file, static_cast<severity_level>(log_level));
    }

    if (!config_file.empty()) {
      std::ifstream ifs(config_file.c_str());
      if (!ifs) {
        throw ParametersException("cannot open config file: " + config_file);
      }
      po::store(po::parse_config_file(ifs, config_file_options), vm);
      notify(vm);
    } else {
      L_(info) << "No config file provided. " << log_file;
    }

    if (vm.count("help") != 0u) {
      std::cout << cmdline_options << "\n";
      exit(EXIT_SUCCESS);
    }

    L_(info) << "Device config:";

    if (vm.count("pci-addr") != 0u) {
      _pci_addr = vm["pci-addr"].as<pci_addr>();
      _dev_autodetect = false;
      L_(info) << " CRI address: " << std::hex << std::setw(2)
               << std::setfill('0') << static_cast<unsigned>(_pci_addr.bus)
               << ":" << std::setw(2) << std::setfill('0')
               << static_cast<unsigned>(_pci_addr.dev) << "."
               << static_cast<unsigned>(_pci_addr.func) << std::dec;
    } else {
      _dev_autodetect = true;
      L_(info) << " CRI address: autodetect";
    }

    if (vm.count("mc-size") != 0u) {
      _mc_size = vm["mc-size"].as<uint32_t>();
      if (_mc_size > (2147483647 / cri::pgen_base_size_ns)) { // 31 bit check
        throw ParametersException("Pgen microslice size out of range");
      }
      L_(info) << " Pgen microslice size: " << human_readable_mc_size(_mc_size);

    } else {
      L_(info) << " Pgen microslice size: " << human_readable_mc_size(_mc_size)
               << " (default)";
    }

    if (vm.count("pgen-rate") != 0u) {
      _pgen_rate = vm["pgen-rate"].as<float>();
      if (_pgen_rate < 0 || _pgen_rate > 1) { // range check
        throw ParametersException("Pgen rate out of range");
      }
      L_(info) << " Pgen rate: " << _pgen_rate;
    }

    //    L_(info) << " CRI microslice size limit: " << _mc_size_limit << "
    //    bytes";

    for (size_t i = 0; i < _num_max_channels; ++i) {
      L_(info) << "Channel " << i << " config:";
      parse_data_source(vm, i);
    } // end loop over channels
  }

  static std::string human_readable_mc_size(uint32_t mc_size) {
    size_t mc_size_ns = mc_size * cri::pgen_base_size_ns;
    size_t mc_size_us_int = mc_size_ns / 1000;
    size_t mc_size_us_frec = mc_size_ns % 1000;
    double mc_freq_khz = 1.e6 / mc_size_ns;
    std::stringstream ss;
    ss << mc_size_us_int << "." << mc_size_us_frec << " us (" << mc_freq_khz
       << " kHz)";
    return ss.str();
  }

  bool _dev_autodetect = true;
  pci_addr _pci_addr = {};
  bool _identify = false;
  uint32_t _mc_size = 10; // 10,24 us
  float _pgen_rate = 1;
  uint32_t _mc_size_limit = 1 << 20; // 1MB
  std::array<struct channel_config, _num_max_channels> _channels = {{}};
};
