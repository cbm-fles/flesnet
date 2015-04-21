/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#pragma once

#include <sys/mman.h>

#include <pda/pci_bar.hpp>
#include <register_file.hpp>

namespace flib {

class register_file_bar : public register_file {

public:
  register_file_bar(pda::pci_bar* bar, sys_bus_addr base_addr);

  int mem(sys_bus_addr addr, void* dest, size_t dwords) override;
  int set_mem(sys_bus_addr addr, const void* source, size_t dwords) override;

protected:
  uint32_t* m_bar; // 32 bit addressing
  size_t m_bar_size;
  sys_bus_addr m_base_addr;
};
  
} // namespace
