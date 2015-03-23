/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#ifndef LIBFLIB_BAR_H
#define LIBFLIB_BAR_H

#include <pthread.h>
#include <cstdint>

typedef struct PciDevice_struct PciDevice;
typedef struct Bar_struct Bar;

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
  ~pci_bar(){};

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
} /** namespace flib */

#endif /* LIBFLIB_BAR_H */
