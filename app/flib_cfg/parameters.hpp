/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "flib.h"
#include "log.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace po = boost::program_options;

static const size_t _num_flib_links = 8;

typedef enum { flim, pgen_far, pgen_near, disable } data_source;

struct link_config {
  data_source source;
  uint16_t eq_id;
};

class parameters {

public:
  parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  parameters(const parameters&) = delete;
  void operator=(const parameters&) = delete;

  size_t flib() const { return _flib; }
  uint32_t mc_size() const { return _mc_size; }
  float pgen_rate() const { return _pgen_rate; }

  struct link_config link(size_t i) const {
    return _links.at(i);
  }

private:
  void parse_eq_id(po::variables_map vm, size_t i) {
    if (vm.count("l" + std::to_string(i) + "_eq_id")) {
      _links.at(i).eq_id = boost::numeric_cast<uint16_t>(
          std::stoul(vm["l" + std::to_string(i) + "_eq_id"].as<std::string>(),
                     nullptr, 0));
    } else {
      std::cerr << " If reading from pgen please provide eq_id.\n";
      exit(EXIT_FAILURE);
    }
  }

  void parse_data_source(po::variables_map vm, size_t i) {
    if (vm.count("l" + std::to_string(i) + "_source")) { // set given parameters
      std::string source =
          vm["l" + std::to_string(i) + "_source"].as<std::string>();
      if (source == "flim") {
        _links.at(i).source = flim;
        std::cout << " data source: flim" << std::endl;
      } else if (source == "pgen_far") {
        _links.at(i).source = pgen_far;
        std::cout << " data source: far-end pgen" << std::endl;
        parse_eq_id(vm, i);
      } else if (source == "pgen_near") {
        _links.at(i).source = pgen_near;
        std::cout << " data source: near-end pgen" << std::endl;
        parse_eq_id(vm, i);
      } else if (source == "disable") {
        _links.at(i).source = disable;
        std::cout << " data source: disable" << std::endl;
      } else {
        std::cerr << " No valid arg for data source." << std::endl;
        exit(EXIT_FAILURE);
      }
    } else { // set default parameters
      _links.at(i).source = disable;
      std::cout << " data source: disable (default)" << std::endl;
    }
  }

  void parse_options(int argc, char* argv[]) {

    std::string config_file;
    unsigned log_level;

    po::options_description generic("Generic options");
    generic.add_options()("help,h", "produce help message")(
        "config-file,c",
        po::value<std::string>(&config_file)->default_value("flib.cfg"),
        "name of a configuration file")(
        "log-level,l", po::value<unsigned>(&log_level)->default_value(2),
        "set the log level (all:0)");

    po::options_description config("Configuration (flib.cfg or cmd line)");
    config.add_options()("flib,i", po::value<size_t>(&_flib)->default_value(0),
                         "index of the target flib")(
        "mc-size,t", po::value<uint32_t>(),
        "size of pattern generator microslices in units of "
        "1024 ns (31 bit wide)")("pgen-rate,r", po::value<float>(),
                                 "MS fill level of pattern generator in [0,1]")

        ("l0_source", po::value<std::string>(),
         "Link 0 data source <disable|flim|pgen_far|pgen_near>")(
            "l0_eq_id", po::value<std::string>(),
            "Equipment identifier of link 0 pgen data source (16 Bit)")(
            "l1_source", po::value<std::string>(),
            "Link 1 data source <disable|flim|pgen_far|pgen_near>")(
            "l1_eq_id", po::value<std::string>(),
            "Equipment identifier of link 1 pgen data source (16 Bit)")(
            "l2_source", po::value<std::string>(),
            "Link 2 data source <disable|flim|pgen_far|pgen_near>")(
            "l2_eq_id", po::value<std::string>(),
            "Equipment identifier of link 2 pgen data source (16 Bit)")(
            "l3_source", po::value<std::string>(),
            "Link 3 data source <disable|flim|pgen_far|pgen_near>")(
            "l3_eq_id", po::value<std::string>(),
            "Equipment identifier of link 3 pgen data source (16 Bit)")(
            "l4_source", po::value<std::string>(),
            "Link 4 data source <disable|flim|pgen_far|pgen_near>")(
            "l4_eq_id", po::value<std::string>(),
            "Equipment identifier of link 4 pgen data source (16 Bit)")(
            "l5_source", po::value<std::string>(),
            "Link 5 data source <disable|flim|pgen_far|pgen_near>")(
            "l5_eq_id", po::value<std::string>(),
            "Equipment identifier of link 5 pgen data source (16 Bit)")(
            "l6_source", po::value<std::string>(),
            "Link 6 data source <disable|flim|pgen_far|pgen_near>")(
            "l6_eq_id", po::value<std::string>(),
            "Equipment identifier of link 6 pgen data source (16 Bit)")(
            "l7_source", po::value<std::string>(),
            "Link 7 data source <disable|flim|pgen_far|pgen_near>")(
            "l7_eq_id", po::value<std::string>(),
            "Equipment identifier of link 7 pgen data source (16 Bit)");

    po::options_description cmdline_options("Allowed options");
    cmdline_options.add(generic).add(config);

    po::options_description config_file_options;
    config_file_options.add(config);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    std::ifstream ifs(config_file.c_str());
    if (!ifs) {
      std::cerr << "can not open config file: " << config_file << "\n";
      exit(EXIT_FAILURE);
    } else {
      po::store(po::parse_config_file(ifs, config_file_options), vm);
      notify(vm);
    }

    if (vm.count("help")) {
      std::cout << cmdline_options << "\n";
      exit(EXIT_SUCCESS);
    }

    logging::add_console(static_cast<severity_level>(log_level));

    std::cout << "Global config:" << std::endl;
    if (vm.count("mc-size")) {
      _mc_size = vm["mc-size"].as<uint32_t>();
      if (_mc_size > 2147483647) { // 31 bit check
        std::cerr << " Pgen microslice size out of range" << std::endl;
        exit(EXIT_FAILURE);
      } else {
        std::cout << " Pgen microslice size: "
                  << human_readable_mc_size(_mc_size) << std::endl;
      }
    } else {
      std::cout << " Pgen microslice size: " << human_readable_mc_size(_mc_size)
                << " (default)" << std::endl;
    }

    if (vm.count("pgen-rate")) {
      _pgen_rate = vm["pgen-rate"].as<float>();
      if (_pgen_rate < 0 || _pgen_rate > 1) { // range check
        std::cerr << " Pgen rate out of range" << std::endl;
        exit(EXIT_FAILURE);
      } else {
        std::cout << " Pgen rate: " << _pgen_rate << "\n";
      }
    }

    for (size_t i = 0; i < _num_flib_links; ++i) {
      std::cout << "Link " << i << " config:" << std::endl;
      parse_data_source(vm, i);
    } // end loop over links
  }

  std::string human_readable_mc_size(uint32_t mc_size) {
    size_t mc_size_ns = mc_size * 1024; // 1024 = hw base unit
    size_t mc_size_us_int = mc_size_ns / 1000;
    size_t mc_size_us_frec = mc_size_ns % 1000;
    double mc_freq_khz = 1.e6 / mc_size_ns;
    std::stringstream ss;
    ss << mc_size_us_int << "." << mc_size_us_frec << " us (" << mc_freq_khz
       << " kHz)";
    return ss.str();
  }

  size_t _flib = 0;
  uint32_t _mc_size = 10; // 10,24 us
  float _pgen_rate = 1;
  std::array<struct link_config, _num_flib_links> _links = {{}};
};
