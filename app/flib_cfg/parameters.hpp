#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <boost/program_options.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <fstream>
#include <flib.h>

#include <log.hpp>

namespace po = boost::program_options;

static const size_t _num_flib_links = 8;  

struct link_config {
  flib2::flib_link::data_sel_t rx_sel;
  flib2::hdr_config hdr_config;
};

class parameters {

public:

  parameters(int argc, char* argv[]) { parse_options(argc, argv); }
  
  parameters(const parameters&) = delete;
  void operator=(const parameters&) = delete;

  uint32_t mc_size() const { return _mc_size; }
  bool debug_mode() const { return _debug_mode; }
  
  struct link_config link_config(size_t i) const { return _link_config.at(i); }

private:
  
  void parse_options(int argc, char* argv[]) {
    
    std::string config_file;
    unsigned log_level;

    po::options_description generic("Generic options");
    generic.add_options()
      ("help,h", "produce help message")
      ("config-file,c", po::value<std::string>(&config_file)->default_value("flib.cfg"),
       "name of a configuration file")
      ("log-level,l", po::value<unsigned>(&log_level)->default_value(2),
       "set the log level (all:0)")
      ;

    po::options_description config("Configuration (flib.cfg or cmd line)");
    config.add_options()
      ("mc-size,t", po::value<uint32_t>(),
       "global size of microslices in units of 8 ns (31 bit wide)")
      ("debug-mode", po::value<bool>(&_debug_mode)->default_value(false),
       "enable readout debug mode")
            
      ("l0_source", po::value<std::string>(), 
       "Link 0 data source <disable|link|pgen|emu>")
      ("l0_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 0 data source (8 Bit)")
      ("l0_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 0 data source (8 Bit)")
      
      ("l1_source", po::value<std::string>(), 
       "Link 1 data source <disable|link|pgen|emu>")
      ("l1_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 1 data source (8 Bit)")
      ("l1_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 1 data source (8 Bit)")
      
      ("l2_source", po::value<std::string>(), 
       "Link 2 data source <disable|link|pgen|emu>")
      ("l2_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 2 data source (8 Bit)")
      ("l2_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 2 data source (8 Bit)")
      
      ("l3_source", po::value<std::string>(), 
       "Link 3 data source <disable|link|pgen|emu>")
      ("l3_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 3 data source (8 Bit)")
      ("l3_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 3 data source (8 Bit)")

      ("l4_source", po::value<std::string>(), 
       "Link 4 data source <disable|link|pgen|emu>")
      ("l4_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 4 data source (8 Bit)")
      ("l4_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 4 data source (8 Bit)")

      ("l5_source", po::value<std::string>(), 
       "Link 5 data source <disable|link|pgen|emu>")
      ("l5_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 5 data source (8 Bit)")
      ("l5_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 5 data source (8 Bit)")

      ("l6_source", po::value<std::string>(), 
       "Link 6 data source <disable|link|pgen|emu>")
      ("l6_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 6 data source (8 Bit)")
      ("l6_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 6 data source (8 Bit)")

      ("l7_source", po::value<std::string>(), 
       "Link 7 data source <disable|link|pgen|emu>")
      ("l7_sys_id", po::value<std::string>(),
       "Subsystem identifier of link 7 data source (8 Bit)")
      ("l7_sys_ver", po::value<std::string>(),
       "Subsystem format version of link 7 data source (8 Bit)")
      ;

    po::options_description cmdline_options("Allowed options");
    cmdline_options.add(generic).add(config);

    po::options_description config_file_options;
    config_file_options.add(config);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);    

    std::ifstream ifs(config_file.c_str());
    if (!ifs)
      {
        std::cout << "can not open config file: " << config_file << "\n";
        exit(EXIT_SUCCESS);
      }
    else
      {
        po::store(po::parse_config_file(ifs, config_file_options), vm);
        notify(vm);
      }

    if (vm.count("help")) {
      std::cout << cmdline_options << "\n";
      exit(EXIT_SUCCESS);
    }
    
    logging::add_console(static_cast<severity_level>(log_level));

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

    for (size_t i = 0; i < _num_flib_links; ++i) {
      std::cout << "Link " << i << " config:" << std::endl;
      
      if (vm.count("l" + std::to_string(i) + "_source")) { // set given parameters
        std::string source = vm["l" + std::to_string(i) + "_source"].as<std::string>();
        
        if ( source == "link" ) {
          _link_config.at(i).rx_sel = flib2::flib_link::rx_link;
          std::cout << " data source: link" << std::endl;
          if (vm.count("l" + std::to_string(i) + "_sys_id") && vm.count("l" + std::to_string(i) + "_sys_ver")) {
            _link_config.at(i).hdr_config.sys_id = boost::numeric_cast<uint8_t>
              (std::stoul(vm["l" + std::to_string(i) + "_sys_id"].as<std::string>(),nullptr,0));
            _link_config.at(i).hdr_config.sys_ver = boost::numeric_cast<uint8_t>
              (std::stoul(vm["l" + std::to_string(i) + "_sys_ver"].as<std::string>(),nullptr,0));
            std::cout << std::hex <<
              " sys_id:      0x" << (uint32_t)_link_config.at(i).hdr_config.sys_id << "\n" <<
              " sys_ver:     0x" << (uint32_t)_link_config.at(i).hdr_config.sys_ver << std::endl;
          } else {
            std::cout << 
              " If reading from 'link' please provide sys_id and sys_ver.\n";
            exit(EXIT_SUCCESS);
          }
        
        } else if (source == "pgen") {
          _link_config.at(i).rx_sel = flib2::flib_link::rx_pgen;
          std::cout << " data source: pgen" << std::endl;
          _link_config.at(i).hdr_config.sys_id = 0xF0;
          _link_config.at(i).hdr_config.sys_ver = 0x01;
        
        } else if (source == "disable") {
          _link_config.at(i).rx_sel = flib2::flib_link::rx_disable;
          std::cout << " data source: disable" << std::endl;
          _link_config.at(i).hdr_config.sys_id = 0xF2;
          _link_config.at(i).hdr_config.sys_ver = 0x01;
        
        } else if (source == "emu") {
          _link_config.at(i).rx_sel = flib2::flib_link::rx_emu;
          std::cout << " data source: emu" << std::endl;
          _link_config.at(i).hdr_config.sys_id = 0xF1;
          _link_config.at(i).hdr_config.sys_ver = 0x01;
        
        } else {
          std::cout << " No valid arg for data source." << std::endl;
          exit(EXIT_SUCCESS);
        }
      
      } else { // set default parameters
        _link_config.at(i).rx_sel = flib2::flib_link::rx_disable;
        std::cout << " data source: disable (default)" << std::endl;
        _link_config.at(i).hdr_config.sys_id = 0xF2;
        _link_config.at(i).hdr_config.sys_ver = 0x01;        
      }
    
    } // end loop over links
  }
    
  uint32_t _mc_size = 125; // 1 us
  std::array<struct link_config, _num_flib_links> _link_config;
  bool _debug_mode = false;

};
