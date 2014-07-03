/**
 * @file
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2011-08-16
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * The rorcfs_device represents a RORC PCIe device
 */

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

/** conditional debug printout command **/
#ifdef DEBUG
#define librorc_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define librorc_debug(fmt, ...)
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
   * Initialize device. This has to be done before using
   * any other member funtion.
   * @param n device-number to be used. n is the n-th directory
   *        in /sys/module/rorcfs/drivers/pci:rorcfs/ with name
   *        0000:[Bus-ID]:[Slot-ID].[Function-ID] as returned by
   *        scandir() with alphasort().
   * @return 0 on sucess, -1 on error or if device does not exist
   **/
  int init(int n = 0);

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

//  /**
//   * get SysFS directory name
//   * @param ptr char** pointer for the directory name
//   * @return size of directory name
//   **/
//  int getDName(char** ptr);



protected:
    char* dname;
    int dname_size;
    uint8 bus;
    uint8 slot;
    uint8 func;

    int find_rorc(char* basedir, struct dirent*** namelist);
    void free_namelist(struct dirent** namelist, int n);

    /*NEW*/
    DeviceOperator *m_dop;
    PciDevice      *m_device;
};

#endif
