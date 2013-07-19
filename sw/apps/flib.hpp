#ifndef FLIB_HPP
#define FILB_HPP

#include <iostream>
#include <iomanip>
#include <array>

#include "librorc.h"
#include "cbm_link.hpp"

class flib {
  
  rorcfs_device* _dev = NULL;
  rorcfs_bar* _bar = NULL;
    
public:

  static const size_t _max_links = 8; // HW constatant
  std::array<cbm_link*,_max_links> link = {{nullptr}};  

  
  flib(int device_nr) {
    // init device class
    _dev = new rorcfs_device();
    if (_dev->init(device_nr) == -1) {
      std::cout << "failed to initialize device 0" << std::endl;
      throw 1;
    }

    // bind to BAR1
    _bar = new rorcfs_bar(_dev, 1);
    if ( _bar->init() == -1 ) {
      std::cout << "BAR1 init failed" << std::endl;
      throw 1;
    }
  }
  
  ~flib() {
    remove_links();
    delete _bar;
    delete _dev;
  }
  

  void add_link(uint32_t i) {
    link[i] = new cbm_link();
    if( link[i]->init(i, _dev, _bar) < 0) {
      std::cout << "link" << i << "init failed" << std::endl;
      throw 1;
    }
  }
 
  void add_links() {
    for (size_t i=0; i<_max_links; i++) {
      if (link[i] == NULL) {
        add_link(i);
      }
    }  
  }
  
  void add_links(std::array<bool,_max_links> mask) {
    for (size_t i=0; i<_max_links; i++) {
      if (mask[i] == true && link[i] == NULL) {
        add_link(i);
      }
    }  
  }
  
  void remove_link(uint32_t i) {
    delete link[i];
    link[i] = NULL;
  }

  void remove_links() {
    for (size_t i=0; i<_max_links; i++) {
      if (link[i] != NULL) {
        remove_link(i);
      }
    }  
  }
  
  void remove_links(std::array<bool,_max_links> mask) {
    for (size_t i=0; i<_max_links; i++) {
      if (mask[i] == true && link[i] != NULL) {
        remove_link(i);
      }
    }  
  }

  void enable_mc_cnt(bool enable) {
    _bar->set_bit(RORC_REG_MC_CNT_CFG, 31, enable);
  }

  void get_fwdate() {
    std::cout << "Firmware Date:" 
              << std::hex << std::showbase << std::setw(8) << std::setfill('0') 
              << _bar->get(RORC_REG_FIRMWARE_DATE) 
              << std::endl;
  }

  void get_devinfo() {
    printf("Bus %x, Slot %x, Func %x\n", _dev->getBus(),
           _dev->getSlot(),_dev->getFunc());
  }

  uint32_t get_reg(uint64_t addr) {
    return _bar->get(addr);
  }

  void set_reg(uint64_t addr, uint32_t data) {
    _bar->set(addr, data);
  }
  
  struct buffs {
    uint64_t* eb; 
    rb_entry* rb;
  };

  std::array<buffs,_max_links> get_buffs() {
    std::array<buffs,_max_links> buf = {};
    for (size_t i=0; i<_max_links; i++) {
      if (link[i] != NULL) {
        buf[i].eb = (uint64_t *)link[i]->ebuf()->getMem();
        buf[i].rb = (struct rb_entry *)link[i]->rbuf()->getMem();
      }
    }
    return buf;
  }


};

#endif // FLIB_HPP
