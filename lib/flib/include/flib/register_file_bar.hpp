// Copyright 2014 Dirk Hutter
// Interface class to access register files via bar memory transfers
#ifndef REGISTER_FILE_BAR_HPP
#define REGISTER_FILE_BAR_HPP

#include <sys/mman.h>
#include "register_file.hpp"

namespace flib {

class register_file_bar : public register_file {
  
private:
  
  uint32_t*    _bar; // 32 bit addressing
  size_t       _bar_size;
  sys_bus_addr _base_addr;

public:

  // Constructor
  register_file_bar(rorcfs_bar* bar, sys_bus_addr base_addr) 
    : _base_addr(base_addr) {
    _bar = static_cast<uint32_t*>(bar->get_mem_ptr());
    _bar_size = bar->get_size();
  }

  int get_mem(sys_bus_addr addr, void *dest, size_t dwords) override {
    // sys_bus hw only supports single 32 bit reads
    sys_bus_addr sys_addr = _base_addr + addr;
    if( ((sys_addr + dwords) << 2) < _bar_size) {      
      for (size_t i = 0; i < dwords; i++) {
        *(static_cast<uint32_t*>(dest) + i) = _bar[sys_addr + i];
      }
      return 0;
    } else {
      return -1;
    }        
  }
  
  int set_mem(sys_bus_addr addr, const void *source, size_t dwords) override {
    sys_bus_addr sys_addr = _base_addr + addr;
    if( ((sys_addr + dwords) << 2) < _bar_size) {      
      // memcpy(reinterpret_cast<uint8_t*>(_bar) + (sys_addr<<2), source, dwords<<2);
      // TODO: don't use memcpy to prevent double writes in
      // conjuction with SSE/SSE2 register usage.
      // See Intel Software Developer’s Manual Vol. 3A 11-8
      for (size_t i = 0; i < dwords; i++) {
       _bar[sys_addr + i] =  *(static_cast<const uint32_t*>(source) + i);
      }
      return msync( (reinterpret_cast<uint8_t*>(_bar) + ((sys_addr<<2) & PAGE_MASK)), PAGE_SIZE, MS_SYNC);
    } else {
      return -1;
    }
  }

};
} // namespace flib
#endif // REGISTER_FILE_BAR_HPP
