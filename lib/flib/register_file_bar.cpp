/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "register_file_bar.hpp"
#include "pda/data_structures.hpp"
#include <iostream>
#include <sys/mman.h>

namespace flib {
register_file_bar::register_file_bar(pda::pci_bar* bar, sys_bus_addr base_addr)
    : m_base_addr(base_addr) {
  m_bar = static_cast<uint32_t*>(bar->mem_ptr());
  m_bar_size = bar->size();
}

__attribute__((__target__("no-sse"))) int
register_file_bar::get_mem(sys_bus_addr addr, void* dest, size_t dwords) {
  // sys_bus hw only supports single 32 bit reads
  sys_bus_addr sys_addr = m_base_addr + addr;
  if (((sys_addr + dwords) << 2) < m_bar_size) {
    for (size_t i = 0; i < dwords; i++) {
      *(static_cast<uint32_t*>(dest) + i) = m_bar[sys_addr + i];
    }
    return 0;
  }
  return -1;
}

__attribute__((__target__("no-sse"))) int register_file_bar::set_mem(
    sys_bus_addr addr, const void* source, size_t dwords) {
  sys_bus_addr sys_addr = m_base_addr + addr;
  if (((sys_addr + dwords) << 2) < m_bar_size) {
    // memcpy(reinterpret_cast<uint8_t*>(_bar) + (sys_addr<<2), source,
    // dwords<<2);
    // TODO: don't use memcpy to prevent double writes in
    // conjuction with SSE/SSE2 register usage.
    // See Intel Software Developer’s Manual Vol. 3A 11-8
    for (size_t i = 0; i < dwords; i++) {
      m_bar[sys_addr + i] = *(static_cast<const uint32_t*>(source) + i);
      if (false) {
        std::cout << "BAR write addr " << sys_addr + i << " data "
                  << static_cast<const uint32_t*>(source)[i] << std::endl;
      }
    }
    return msync(
        (reinterpret_cast<uint8_t*>(m_bar) + ((sys_addr << 2) & PAGE_MASK)),
        PAGE_SIZE, MS_SYNC);
  }
  return -1;
}

} // namespace flib
