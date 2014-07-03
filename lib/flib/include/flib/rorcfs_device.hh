/**
 *
 * @file
 * @author Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @date 2014-07-03
 *
 **/

#include <pda.h>

#define DEVICE_CONSTRUCTOR_FAILED 0

#ifndef _RORCLIB_RORCFS_DEVICE_H
#define _RORCLIB_RORCFS_DEVICE_H

#ifndef uint8
/**
 * @typedef uint8
 * @brief typedef for unsigned 8 bit IDs
 **/
typedef unsigned char uint8;
#endif


/**
 * @class rorcfs_device
 * @brief represents a RORC PCIe device
 *
 * The rorcfs_device class is the base class for all device
 * IO. Create a rorcfs_device instance and initialize
 * (via init(int n)) with the device you want to bind to.
 * Once the device is sucessfully initialized you can attach
 * instances of rorcfs_bar.
 **/
class device
{

public:
   device();
  ~device();

  /**
   * get PCIe Bus-ID
   * @return uint8 Bus-ID
   **/
  uint8 getBus() { return bus; }

  /**
   * get PCIe Slot-ID
   * @return uint8 Slot-ID
   **/
  uint8 getSlot() { return slot; }

  /**
   * get PCIe Function-ID
   * @return uint8 Function-ID
   **/
  uint8 getFunc() { return func; }

protected:

    uint8 bus;
    uint8 slot;
    uint8 func;

    /*NEW*/
    DeviceOperator *m_dop;
    PciDevice      *m_device;
};

#endif
