#ifndef FLIB_DEVICE_HPP
#define FLIB_DEVICE_HPP

#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include "boost/date_time/posix_time/posix_time.hpp" //include all types plus i/o

#include "flib_link.hpp"
#include "register_file_bar.hpp"

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace flib {

 constexpr std::array<uint16_t, 1> hw_ver_table = {{ 4}};

class flib_device {

private:

  std::unique_ptr<rorcfs_device> _dev;
  std::unique_ptr<rorcfs_bar> _bar;
  std::unique_ptr<register_file_bar> _rf;

  uint8_t _get_num_hw_links() {
    uint8_t n = (_rf->get_reg(RORC_REG_N_CHANNELS) & 0xFF);
    return n;
  }

  bool _check_magic_number() {
    bool match = false;
    if ((_rf->get_reg(0) & 0xFFFF) == 0x4844) { // RORC_REG_HARDWARE_INFO
      match = true;
    }
    return match;
  }

  bool _check_hw_ver() {
    uint16_t hw_ver = _rf->get_reg(0) >> 16; // RORC_REG_HARDWARE_INFO;
    bool match = false;
    // check if version of hardware is part of suported versions
    for (auto it = hw_ver_table.begin(); it != hw_ver_table.end() && match == false; ++it) {
      if (hw_ver == *it) {
        match = true;
      }
    }
    // check if version of hardware matches exactly version of header
    if (hw_ver != RORC_C_HARDWARE_VERSION) {
      match = false;
    }
    return match;
  }

public:
  std::vector<std::unique_ptr<flib_link> > link;

  // default constructor
  flib_device(int device_nr) {
    // init device class
    _dev = std::unique_ptr<rorcfs_device>(new rorcfs_device());
    if (_dev->init(device_nr) == -1) {
      throw RorcfsException("Failed to initialize device");
    }
    // bind to BAR1
    _bar = std::unique_ptr<rorcfs_bar>(new rorcfs_bar(_dev.get(), 1));
    if ( _bar->init() == -1 ) {
      throw RorcfsException("BAR1 init failed");
    }
    // register file access
    _rf = std::unique_ptr<register_file_bar>(new register_file_bar(_bar.get(), 0));

    // enforce correct magic numebr and hw version
    if (!_check_magic_number()) {
      throw FlibException("Can not read magic number! \n Try to reinitialize FLIB.");
    }
    if (!_check_hw_ver()) {
      throw FlibException("Hardware - libflib version missmatch!");
    }
    // create link objects
    uint8_t num_links = _get_num_hw_links();
    for (size_t i=0; i<num_links; i++) {
      link.push_back(
        std::unique_ptr<flib_link>(
          new flib_link(i, _dev.get(), _bar.get() )));
    }
  }
  
  // destructor
  ~flib_device() { }
  
  bool check_hw_ver() {
    return _check_hw_ver();
  }
  
  size_t get_num_links() {
    return link.size();
  }

  flib_link& get_link(size_t n) {
    return *link.at(n);
  }

  std::vector<flib_link*> get_links() {
    std::vector<flib_link*> links;
    for (auto& l : link) {
      links.push_back(l.get());
    }
    return links;
  }
  
  register_file_bar* get_rf() const {
    return _rf.get();
  }

  void enable_mc_cnt(bool enable) {
    _rf->set_bit(RORC_REG_MC_CNT_CFG, 31, enable);
  }

  void set_mc_time(uint32_t time) {
    // time: 31 bit wide, in units of 8 ns
    uint32_t reg = _rf->get_reg(RORC_REG_MC_CNT_CFG);
    reg = (reg & ~0x7FFFFFFF) | (time & 0x7FFFFFFF);
    _rf->set_reg(RORC_REG_MC_CNT_CFG, reg);
  }

  // global dlm send, rquires link local prepare_dlm beforehand
  void send_dlm() {
    _rf->set_reg(RORC_REG_DLM_CFG, 1);
  }

  boost::posix_time::ptime get_build_date() {
    time_t time = 
      static_cast<time_t>(_rf->get_reg(RORC_REG_BUILD_DATE_L)
                          | (static_cast<uint64_t>(_rf->get_reg(RORC_REG_BUILD_DATE_H))<<32));
    boost::posix_time::ptime t = boost::posix_time::from_time_t(time);
    return t;
  }

   uint16_t get_hw_ver() {
    uint16_t ver = static_cast<uint16_t>(_rf->get_reg(0) >> 16); // RORC_REG_HARDWARE_INFO
    return ver;
  }

  struct build_info {
    boost::posix_time::ptime date;
    uint32_t rev[5];
    uint16_t hw_ver;
    bool clean;
  };

  struct build_info get_build_info() {
    build_info info;
    
    info.date = get_build_date();
    info.rev[0] = _rf->get_reg(RORC_REG_BUILD_REV_0);
    info.rev[1] = _rf->get_reg(RORC_REG_BUILD_REV_1);
    info.rev[2] = _rf->get_reg(RORC_REG_BUILD_REV_2);
    info.rev[3] = _rf->get_reg(RORC_REG_BUILD_REV_3);
    info.rev[4] = _rf->get_reg(RORC_REG_BUILD_REV_4);
    info.hw_ver = get_hw_ver();
    info.clean = (_rf->get_reg(RORC_REG_BUILD_FLAGS) & 0x1);
    return info;
  }

  std::string print_build_info() {
    build_info build = get_build_info();
    std::stringstream ss;
    ss << "Build Date:     " << build.date << std::endl
       << "Repository Revision: " << std::hex
       << build.rev[4] << build.rev[3] << build.rev[2]
       << build.rev[1] << build.rev[0] << std::endl;
    if (build.clean)
      ss << "Repository Status:   clean " << std::endl;
    else
      ss << "Repository Status:   NOT clean " << std::endl
         << "Hardware Version:    " << build.hw_ver;
    return ss.str();
  }
  
  std::string get_devinfo() {
    std::stringstream ss;
    ss << "Bus "  << static_cast<uint32_t>(_dev->getBus())
       << " Slot " << static_cast<uint32_t>(_dev->getSlot())
       << " Func " << static_cast<uint32_t>(_dev->getFunc());
    return ss.str();
  }

};

} // namespace flib

// #pragma GCC diagnostic pop

#endif // FLIB_DEVICE_HPP
