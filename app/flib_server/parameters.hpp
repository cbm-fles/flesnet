#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <boost/program_options.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <fstream>
#include <flib.h>

#include "log.hpp"
#include "Utility.hpp"
#include "MicrosliceDescriptor.hpp"

namespace po = boost::program_options;

static const size_t _num_flib_links = 8;  

typedef struct {
  flib::flib_link::data_sel_t rx_sel;
  flib::flib_link::hdr_config_t hdr_config;
} link_config_t;

class parameters {

public:

  parameters(int argc, char* argv[]) { parse_options(argc, argv); }
  
  parameters(const parameters&) = delete;
  void operator=(const parameters&) = delete;

  size_t data_buffer_size_exp() { return _data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return _desc_buffer_size_exp; }
  
  uint32_t mc_size() const { return _mc_size; }
  bool debug_mode() const { return _debug_mode; }
  
  link_config_t link_config(size_t i) const { return _link_config.at(i); }

  std::string print_buffer_info() {
    std::stringstream ss;
    ss << "Buffer size per link: "
       << human_readable_count(UINT64_C(1) << _data_buffer_size_exp)
       << " + "
       << human_readable_count((UINT64_C(1) << _desc_buffer_size_exp) *
                               sizeof(fles::MicrosliceDescriptor));
    return ss.str();
  }
  
private:
  
  void parse_options(int argc, char* argv[]) {
    
    std::string config_file;
    unsigned log_level;

    po::options_description generic("Generic options");
    generic.add_options()
      ("help,h", "produce help message")
      ("config-file,c", po::value<std::string>(&config_file)->default_value("flib.cfg"),
       "name of a configuration file")
      ;

    po::options_description config("Configuration (flib.cfg or cmd line)");
    config.add_options()

      ("in-data-buffer-size-exp",
       po::value<size_t>(&_data_buffer_size_exp)->default_value(27),
       "exp. size of the data buffer in bytes")
      ("in-desc-buffer-size-exp",
       po::value<size_t>(&_desc_buffer_size_exp)->default_value(19),
       "exp. size of the descriptor buffer (number of entries)")
      ("log-level,l", po::value<unsigned>(&log_level)->default_value(2),
       "set the log level (all:0)")
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
        L_(error) << "Microslice size out of range";
        exit(EXIT_SUCCESS);
      } else {
        L_(info) << "Microslice size set to " 
           << _mc_size << " * 8 ns.";  
      }
    } else {
      L_(info) << "Microslice size set to default.";
    }

    for (size_t i = 0; i < _num_flib_links; ++i) {
      L_(info) << "Link " << i << " config:";
      
      if (vm.count("l" + std::to_string(i) + "_source")) { // set given parameters
        std::string source = vm["l" + std::to_string(i) + "_source"].as<std::string>();
        
        if ( source == "link" ) {
          _link_config.at(i).rx_sel = flib::flib_link::rx_link;
          L_(info) << " data source: link";
          if (vm.count("l" + std::to_string(i) + "_sys_id") && vm.count("l" + std::to_string(i) + "_sys_ver")) {
            _link_config.at(i).hdr_config.sys_id = boost::numeric_cast<uint8_t>
              (std::stoul(vm["l" + std::to_string(i) + "_sys_id"].as<std::string>(),nullptr,0));
            _link_config.at(i).hdr_config.sys_ver = boost::numeric_cast<uint8_t>
              (std::stoul(vm["l" + std::to_string(i) + "_sys_ver"].as<std::string>(),nullptr,0));
            _link_config.at(i).hdr_config.eq_id = static_cast<uint16_t>(0xE000 + i);
            L_(info) << std::hex <<
              " sys_id:      0x" << (static_cast<uint16_t>(_link_config.at(i).hdr_config.sys_id)) << "\n" <<
              " sys_ver:     0x" << (static_cast<uint16_t>(_link_config.at(i).hdr_config.sys_ver));
          } else {
            L_(error) << 
              " If reading from 'link' please provide sys_id and sys_ver.";
            exit(EXIT_SUCCESS);
          }
        
        } else if (source == "pgen") {
          _link_config.at(i).rx_sel = flib::flib_link::rx_pgen;
          L_(info) << " data source: pgen";
          _link_config.at(i).hdr_config.sys_id = 0xF0;
          _link_config.at(i).hdr_config.sys_ver = 0x01;
          _link_config.at(i).hdr_config.eq_id = static_cast<uint16_t>(0xE000 + i);
        
        } else if (source == "disable") {
          _link_config.at(i).rx_sel = flib::flib_link::rx_disable;
          L_(info) << " data source: disable";
          _link_config.at(i).hdr_config.sys_id = 0xF2;
          _link_config.at(i).hdr_config.sys_ver = 0x01;
          _link_config.at(i).hdr_config.eq_id = static_cast<uint16_t>(0xE000 + i);
        
        } else if (source == "emu") {
          _link_config.at(i).rx_sel = flib::flib_link::rx_emu;
          L_(info) << " data source: emu";
          _link_config.at(i).hdr_config.sys_id = 0xF1;
          _link_config.at(i).hdr_config.sys_ver = 0x01;
          _link_config.at(i).hdr_config.eq_id = static_cast<uint16_t>(0xE000 + i);
        
        } else {
          L_(error) << " No valid arg for data source.";
          exit(EXIT_SUCCESS);
        }
      
      } else { // set default parameters
        _link_config.at(i).rx_sel = flib::flib_link::rx_disable;
        L_(info) << " data source: disable (default)";
        _link_config.at(i).hdr_config.sys_id = 0xF2;
        _link_config.at(i).hdr_config.sys_ver = 0x01;        
        _link_config.at(i).hdr_config.eq_id = static_cast<uint16_t>(0xE000 + i);
      }
    
    } // end loop over links

    L_(info) << print_buffer_info();
  }

  size_t _data_buffer_size_exp;
  size_t _desc_buffer_size_exp;

  uint32_t _mc_size = 125; // 1 us
  std::array<link_config_t, _num_flib_links> _link_config;
  bool _debug_mode = false;

};
