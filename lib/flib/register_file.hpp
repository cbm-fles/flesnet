/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#pragma once

namespace flib {

typedef uint64_t sys_bus_addr;

class register_file {
public:
  virtual ~register_file(){};

  virtual int mem(sys_bus_addr addr, void* dest, size_t dwords) = 0;

  virtual int set_mem(sys_bus_addr addr, const void* source, size_t dwords) = 0;

  virtual uint32_t reg(sys_bus_addr addr) {
    uint32_t val;
    mem(addr, static_cast<void*>(&val), 1);
    return val;
  }

  virtual void set_reg(sys_bus_addr addr, uint32_t data) {
    set_mem(addr, static_cast<const void*>(&data), 1);
  }

  virtual bool bit(sys_bus_addr addr, int pos) {
    uint32_t registr = reg(addr);
    return (registr & (1 << pos));
  }

  virtual void set_bit(sys_bus_addr addr, int pos, bool enable) {
    uint32_t registr = reg(addr);
    if (enable) {
      set_reg(addr, (registr | (1 << pos)));
    } else {
      set_reg(addr, (registr & ~(1 << pos)));
    }
  }
};
} // namespace
