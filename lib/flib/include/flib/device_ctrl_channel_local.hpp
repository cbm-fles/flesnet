// Copyright 2014 Dirk Hutter
#ifndef DEVICE_CTRL_CHANNEL_LOCAL_HPP
#define DEVICE_CTRL_CHANNEL_LOCAL_HPP

#include "rorc_registers.h"
#include "rorcfs_bar.hh"
#include "device_ctrl_channel.hpp"

namespace flib {

class device_ctrl_channel_local : public device_ctrl_channel {
  
private:
  
  rorcfs_bar* _bar;

public:

  // Constructor
  device_ctrl_channel_local(rorcfs_bar* bar) : _bar(bar) {}

  ~device_ctrl_channel_local() {}
  
  size_t get_mem(uint64_t addr, void *dest, size_t n) override {
    // ensure only 4 byte blocks are red
    n = 4 + 4*(n>>2);
    _bar->get_mem(addr, dest, n);
    return n;
  }

  size_t set_mem(uint64_t addr, const void *source, size_t n) override {
    // ensure only 4 byte blocks are written
    n = 4 + 4*(n>>2);
    _bar->set_mem(addr, source, n);
    return n;
  }

  uint32_t get_reg(uint64_t addr) override {
    uint32_t val;
    get_mem(addr, (void*)&val, 4);
    return val;
  }
  
  void set_reg(uint64_t addr, uint32_t data) override {
    set_mem(addr, (const void*)&data, 4);
  }

  bool get_bit(uint64_t addr, int pos) override {
    uint32_t reg = get_reg(addr);
    return (reg & (1<<pos)); 
  }

  void set_bit(uint64_t addr, int pos, bool enable) override {
    uint32_t reg = get_reg(addr);
    if ( enable ) {
      set_reg(addr, (reg | (1<<pos)));
    }
    else {
      set_reg(addr, (reg & ~(1<<pos)));
    }    
  }

};

} // namespace flib
#endif // DEVICE_CTRL_CHANNEL_LOCAL_HPP
