#ifndef FLIB_HPP
#define FILB_HPP

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include "boost/date_time/posix_time/posix_time.hpp" //include all types plus i/o

#include "flib_link.hpp"

namespace flib {

class flib_device {
  
private:

  std::unique_ptr<rorcfs_device> _dev;
  std::unique_ptr<rorcfs_bar> _bar;

  uint8_t _get_num_hw_links() {
    uint8_t n = (_bar->get(RORC_REG_N_CHANNELS) & 0xFF);
    return n;
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
    // create link objects
    uint8_t num_links = _get_num_hw_links();
    for (size_t i=0; i<num_links; i++) {
      link.push_back(std::unique_ptr<flib_link>(new flib_link(i, _dev.get(), _bar.get())));
    }
  }
  
  // destructor
  ~flib_device() { }
  
  
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
  
  void enable_mc_cnt(bool enable) {
    _bar->set_bit(RORC_REG_MC_CNT_CFG, 31, enable);
  }

  void send_dlm() {
    set_reg(RORC_REG_DLM_CFG, 1);
  }

  boost::posix_time::ptime get_build_date() {
    time_t time = static_cast<time_t>(_bar->get(RORC_REG_BUILD_DATE_L) \
                                     | ((uint64_t)(_bar->get(RORC_REG_BUILD_DATE_H))<<32));
    boost::posix_time::ptime t = boost::posix_time::from_time_t(time);
    return t;
  }
 
  struct build_info {
    boost::posix_time::ptime date;
    uint32_t rev[5];
    uint32_t hw_ver;
    bool clean;
  };

  struct build_info get_build_info() {
    build_info info;

    time_t time = static_cast<time_t>(_bar->get(RORC_REG_BUILD_DATE_L) \
                | ((uint64_t)(_bar->get(RORC_REG_BUILD_DATE_H))<<32));
    info.date = boost::posix_time::from_time_t(time);
    info.rev[0] = _bar->get(RORC_REG_BUILD_REV_0);
    info.rev[1] = _bar->get(RORC_REG_BUILD_REV_1);
    info.rev[2] = _bar->get(RORC_REG_BUILD_REV_2);
    info.rev[3] = _bar->get(RORC_REG_BUILD_REV_3);
    info.rev[4] = _bar->get(RORC_REG_BUILD_REV_4);
    info.hw_ver = _bar->get(RORC_REG_HARDWARE_VERSION);
    info.clean = (_bar->get(RORC_REG_BUILD_INFO) & 0x1);
    return info;
  }

  std::string print_build_info() {
    build_info build = get_build_info();
    std::stringstream ss;
    ss << "Build Date:     " << build.date << std::endl
       << "Build Revision: " << std::hex
       << build.rev[4] << build.rev[3] << build.rev[2]
       << build.rev[1] << build.rev[0] << std::endl;
    if (build.clean)
      ss << "Repository was clean " << std::endl;
    else
      ss << "Repository was not clean " << std::endl;
    ss << "Hardware Version: " << build.hw_ver << std::endl;
    return ss.str();
  }
  
  std::string get_devinfo() {
    std::stringstream ss;
    ss << "Bus "  << (uint32_t)_dev->getBus()
       << " Slot " << (uint32_t)_dev->getSlot()
       << " Func " << (uint32_t)_dev->getFunc();
    return ss.str();
  }

  uint32_t get_reg(uint64_t addr) {
    return _bar->get(addr);
  }

  void set_reg(uint64_t addr, uint32_t data) {
    _bar->set(addr, data);
  }
  
};
} // namespace flib
#endif // FLIB_HPP
