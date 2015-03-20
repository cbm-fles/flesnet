/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#ifndef REGISTER_FILE_BAR_HPP
#define REGISTER_FILE_BAR_HPP

#include <sys/mman.h>

#include <register_file.hpp>

namespace pda {
class pci_bar;
}

using namespace pda;

namespace flib {

class register_file_bar : public register_file {

public:
  register_file_bar(pci_bar* bar, sys_bus_addr base_addr);

  int mem(sys_bus_addr addr, void* dest, size_t dwords) override;
  int set_mem(sys_bus_addr addr, const void* source, size_t dwords) override;

protected:
  uint32_t* m_bar; // 32 bit addressing
  size_t m_bar_size;
  sys_bus_addr m_base_addr;
};
}
#endif /** REGISTER_FILE_BAR_HPP */
