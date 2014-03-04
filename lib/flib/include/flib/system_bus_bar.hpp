// Copyright 2014 Dirk Hutter
// Interface class to access system bus via bar memory transfers
#ifndef SYSTEM_BUS_BAR_HPP
#define SYSTEM_BUS_BAR_HPP

#include <sys/mman.h>
#include "system_bus.hpp"

namespace flib {

class system_bus_bar : public system_bus {
  
private:
  
  uint32_t* _bar; // 32 bit addressing
  size_t    _bar_size;

public:

  // Constructor
  // TODO replace rorcfs_bar with more general one
  system_bus_bar(rorcfs_bar* bar) {
    _bar = static_cast<uint32_t*>(bar->get_mem_ptr());
    _bar_size = bar->get_size();
  }

  int get_mem(sys_bus_addr addr, void *dest, size_t dwords) override {
    // sys_bus hw only supports single 32 bit reads
    if( ((addr + dwords) << 2) < _bar_size) {      
      for (size_t i = 0; i < dwords; i++) {
        *(static_cast<uint32_t*>(dest) + i) = _bar[addr+i];
      }
      return 0;
    } else {
      return -1;
    }        
  }
  
  int set_mem(sys_bus_addr addr, const void *source, size_t dwords) override {
    if( ((addr + dwords) << 2) < _bar_size) {      
      memcpy(reinterpret_cast<uint8_t*>(_bar) + (addr<<2), source, dwords<<2);
      return msync( (reinterpret_cast<uint8_t*>(_bar) + ((addr<<2) & PAGE_MASK)), PAGE_SIZE, MS_SYNC);
    } else {
      return -1;
    }
  }

};
} // namespace flib
#endif // SYSTEM_BUS_BAR_HPP
