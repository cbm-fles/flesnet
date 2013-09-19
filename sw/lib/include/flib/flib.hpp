#ifndef FLIB_HPP
#define FILB_HPP

#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>
#include <string>
#include <sstream>

#include "cbm_link.hpp"

class FlibException : public std::runtime_error {
public:
  // default constructor
  explicit FlibException(const std::string& what_arg = "")
    : std::runtime_error(what_arg) { }
};

class RorcfsException : public FlibException {
public:
  // default constructor
  explicit RorcfsException(const std::string& what_arg = "")
    : FlibException(what_arg) { }
};

class flib {
  
  rorcfs_device* _dev = NULL;
  rorcfs_bar* _bar = NULL;
  uint8_t _num_links = 0;

public:

  std::vector<cbm_link*> link;  
  
  flib(int device_nr) {
    // init device class
    _dev = new rorcfs_device();
    if (_dev->init(device_nr) == -1) {
      throw RorcfsException("Failed to initialize device");
    }
    // bind to BAR1
    _bar = new rorcfs_bar(_dev, 1);
    if ( _bar->init() == -1 ) {
      throw RorcfsException("BAR1 init failed");
    }
    // create link objects
    _num_links = get_num_links();
    for (size_t i=0; i<_num_links; i++) {
      link.push_back(new cbm_link(i, _dev, _bar));
    }
  }
  
  ~flib() {
    for (size_t i=0; i<_num_links; i++) {
      delete link[i];
    }
    delete _bar;
    delete _dev;
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
       << " Func " << (uint32_t)_dev->getFunc() << std::endl;
    return ss.str();
  }

  uint8_t get_num_links() {
    uint8_t n = (_bar->get(RORC_REG_N_CHANNELS) & 0xFF);
    return n;
  }

  uint32_t get_reg(uint64_t addr) {
    return _bar->get(addr);
  }

  void set_reg(uint64_t addr, uint32_t data) {
    _bar->set(addr, data);
  }
  
  struct buffs {
    uint64_t* eb; 
     MicrosliceDescriptor* db;
  };

  std::vector<buffs> get_buffs() {
    std::vector<buffs> buf;
    for (size_t i=0; i<_num_links; i++) {
      struct buffs tmp;
      tmp.eb = (uint64_t *)link[i]->ebuf()->getMem();
      tmp.db = (struct  MicrosliceDescriptor *)link[i]->rbuf()->getMem();
      buf.push_back(tmp);
    }
    return buf;
  }


};

#endif // FLIB_HPP
