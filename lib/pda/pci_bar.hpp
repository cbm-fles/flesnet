/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#pragma once

#include <cstdint>
#include <pthread.h>

using PciDevice = struct PciDevice_struct;
using Bar = struct Bar_struct;

namespace pda {
class device;

/**
 * @class
 * @brief Represents a Base Address Register (BAR) file
 * mapping of the FLIBs PCIe address space
 */
class pci_bar {
public:
  /**
   * @param dev parent rorcfs_device
   * @param n number of BAR to be mapped [0-6]
   **/
  pci_bar(device* dev, uint8_t number);
  ~pci_bar() = default;

  void* mem_ptr() { return static_cast<void*>(m_bar); };

  /**
   * Get size of mapped BAR.
   * @return size of mapped BAR in bytes
   **/
  size_t size() { return (m_size); };

protected:
  device* m_parent_dev;
  PciDevice* m_pda_pci_device;
  Bar* m_pda_bar;
  uint8_t m_number;
  uint8_t* m_bar;
  size_t m_size;

  void barMap(uint8_t number);
};
} // namespace pda
