/**
 *
 * @file
 * @author Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @date 2014-07-03
 *
 **/

#include <pda.h>

#define DEVICE_CONSTRUCTOR_FAILED 0

#ifndef DEVICE_H
#define DEVICE_H

#ifndef uint8
/**
 * @typedef uint8
 * @brief typedef for unsigned 8 bit IDs
 **/
typedef unsigned char uint8;
#endif


/**
 * @class
 * @brief Represents a FLIB PCIe device
 **/
class device
{

public:
   device();
  ~device();

  uint16_t getDomain();

  /**
   * get PCIe Bus-ID
   * @return uint8 Bus-ID
   **/
  uint8 getBus();

  /**
   * get PCIe Slot-ID
   * @return uint8 Slot-ID
   **/
  uint8 getSlot();

  /**
   * get PCIe Function-ID
   * @return uint8 Function-ID
   **/
  uint8 getFunc();

protected:
    DeviceOperator *m_dop;
    PciDevice      *m_device;
};

#endif /** DEVICE_H */
