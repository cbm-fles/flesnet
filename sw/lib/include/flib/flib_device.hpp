#ifndef FLIB_HPP
#define FILB_HPP

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <memory>

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

  void enable_mc_cnt(bool enable) {
    _bar->set_bit(RORC_REG_MC_CNT_CFG, 31, enable);
  }

  void send_dlm() {
    set_reg(RORC_REG_DLM_CFG, 1);
  }

  uint32_t get_fwdate() {
    uint32_t date = _bar->get(RORC_REG_FIRMWARE_DATE);
    return date;
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
