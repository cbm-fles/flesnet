/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

namespace flib {

using sys_bus_addr = uint64_t;

class register_file {
public:
  virtual ~register_file() = default;

  virtual int get_mem(sys_bus_addr addr, void* dest, size_t dwords) = 0;

  virtual int set_mem(sys_bus_addr addr, const void* source, size_t dwords) = 0;

  virtual uint32_t get_reg(sys_bus_addr addr) {
    uint32_t val;
    get_mem(addr, static_cast<void*>(&val), 1);
    return val;
  }

  virtual void set_reg(sys_bus_addr addr, uint32_t data) {
    set_mem(addr, static_cast<const void*>(&data), 1);
  }

  virtual void set_reg(sys_bus_addr addr, uint32_t data, uint32_t mask) {
    uint32_t reg = get_reg(addr);
    // clear all unselected bits in input data
    data &= mask;
    // clear all selects bits in reg and set according to input data
    reg &= ~mask;
    reg |= data;
    set_reg(addr, reg);
  }

  virtual bool get_bit(sys_bus_addr addr, int pos) {
    uint32_t reg = get_reg(addr);
    return (reg & (1 << pos)) != 0u;
  }

  virtual void set_bit(sys_bus_addr addr, int pos, bool enable) {
    uint32_t reg = get_reg(addr);
    if (enable) {
      set_reg(addr, (reg | (1 << pos)));
    } else {
      set_reg(addr, (reg & ~(1 << pos)));
    }
  }
};
} // namespace flib
