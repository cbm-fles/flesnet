/**
 * @file rorcfs_bar.hh
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
 * The rorcfs_bar class represents a Base Address Register (BAR) file
 * mapping of the RORCs PCIe address space
 */

#ifndef _RORCLIB_RORCFS_BAR_H
#define _RORCLIB_RORCFS_BAR_H

#include "rorcfs_device.hh"
#include <sys/stat.h>
#include <pthread.h>

/**
 * @class rorcfs_bar
 * @brief Represents a Base Address Register (BAR) file
 * mapping of the RORCs PCIe address space
 *
 * Create a new rorcfs_bar object after initializing your
 * rorcfs_device instance. <br>Once your rorcfs_bar instance is
 * initialized (with init()) you can use get() and set() to
 * read from and/or write to the device.
 */
class rorcfs_bar {
public:
  /**
   * Constructor that sets fname accordingly. No mapping is
   * performed at this point.
   * @param dev parent rorcfs_device
   * @param n number of BAR to be mapped [0-6]
   **/
  rorcfs_bar(rorcfs_device* dev, int n);

  /**
   * Deconstructor: free fname, unmap BAR, close file
   **/
  ~rorcfs_bar();

  /**
   * initialize BAR mapping: open sysfs file, get file stats,
   * mmap file. This has to be done before using any other
   * member funtion. This function will fail if the requested
   * BAR does not exist.
   * @return 0 on sucess, -1 on errors
   **/
  int init();

  void* get_mem_ptr() {
    return static_cast<void*>(bar);
  };

  /**
   * get size of mapped BAR. This value is only valid after init()
   * @return size of mapped BAR in (unsigned long) bytes
   **/
  size_t get_size() {
    return static_cast<size_t>(barstat.st_size);
  };

private:
  int handle;
  char* fname;
  struct stat barstat;
  unsigned int* bar;
  int number;
  pthread_mutex_t mtx;
};

#endif // _RORCLIB_RORCFS_BAR_H
