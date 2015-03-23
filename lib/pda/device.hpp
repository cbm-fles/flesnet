/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#ifndef PCI_DEVICE_H
#define PCI_DEVICE_H

#include <cstdint>

typedef struct DeviceOperator_struct DeviceOperator;
typedef struct PciDevice_struct PciDevice;

namespace pda {
/**
 * @class
 * @brief Represents a FLIB PCIe device
 **/
class device {
  friend class dma_buffer;

public:
  device(int32_t device_index);
  ~device();

  uint16_t domain();

  /**
   * PCIe Bus-ID
   * @return uint8 Bus-ID
  **/
  uint8_t bus();

  /**
   * PCIe Slot-ID
   * @return uint8 Slot-ID
  **/
  uint8_t slot();

  /**
   * PCIe Function-ID
   * @return uint8 Function-ID
  **/
  uint8_t func();

  /**
   * PCI-Device
   * @return PCI-Device-Pointer
  **/
  PciDevice* PDAPciDevice() { return (m_device); }

protected:
  DeviceOperator* m_dop;
  PciDevice* m_device;
};
}

#endif /** DEVICE_H */
