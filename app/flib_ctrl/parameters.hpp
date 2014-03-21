#pragma once

#include <iostream>
#include <string>
#include <boost/program_options.hpp>

#include <flib.h>

namespace po = boost::program_options;

class parameters {

public:

  parameters(int argc, char* argv[]) { parse_options(argc, argv); }
  
  parameters(const parameters&) = delete;
  void operator=(const parameters&) = delete;

  uint32_t mc_size() const { return _mc_size; }
  
  flib::flib_link::data_rx_sel rx_sel() const { return _rx_sel; }

  bool enable_daq() const { return _enable_daq; }

private:
  
  void parse_options(int argc, char* argv[]) {
    
    po::options_description cmdline("Allowed options");
    cmdline.add_options()
      ("help", "produce help message")
      ("mc-size,t", po::value<uint32_t>(),
       "global size of microslices in units of 8 ns (31 bit wide)")
      ("source,s", po::value<std::string>(), 
       "set data source to <disable|link|pgen|emu>")
      ("enable-daq,e", po::value<bool>(&_enable_daq), 
       "send DLM 8 to enable DAQ")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline), vm);
    po::notify(vm);    
    
    if (vm.count("help")) {
      std::cout << cmdline << "\n";
      exit(EXIT_SUCCESS);
    }
    
    if (vm.count("mc-size")) {
      _mc_size = vm["mc-size"].as<uint32_t>();
      if (_mc_size > 2147483647) { // 31 bit check
        std::cout << "Microslice size out of range" << std::endl;
        exit(EXIT_SUCCESS);
      } else {
        std::cout << "Microslice size set to " 
           << _mc_size << " * 8 ns.\n";  
      }
    } else {
      std::cout << "Microslice size set to default.\n";
    }
  
    if (vm.count("source")) {
      std::string source = vm["source"].as<std::string>();
      if ( source == "link" ) {
        _rx_sel = flib::flib_link::link;
        std::cout << "Data source set to link." << std::endl;
      } else if (source == "pgen") {
        _rx_sel = flib::flib_link::pgen;
        std::cout << "Data source set to pgen." << std::endl;
      } else if (source == "disable") {
        _rx_sel = flib::flib_link::disable;
        std::cout << "Data source set to disable." << std::endl;
      } else if (source == "emu") {
        _rx_sel = flib::flib_link::emu;
        std::cout << "Data source set to emu." << std::endl;
      } else {
        std::cout << "No valid arg for source." << std::endl;
        exit(EXIT_SUCCESS);
      }
    }
  }

  uint32_t _mc_size = 125; // 1 us
  flib::flib_link::data_rx_sel _rx_sel = flib::flib_link::pgen;
  bool _enable_daq = false;

};
