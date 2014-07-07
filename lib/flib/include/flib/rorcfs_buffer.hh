/**
 * @file rorcfs_buffer.hh
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2011-08-17
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
 */

#ifndef _RORCLIB_RORCFS_BUFFER_H
#define _RORCLIB_RORCFS_BUFFER_H

#include <flib/rorcfs.h>

namespace flib
{
    class device;

    /**
     * @class rorcfs_buffer
     * @brief buffer management class
     *
     * This class manages the DMA receive buffers. One instance of this
     * class represents one couple of EventBuffer and ReportBuffer with
     * their corresponding sysfs attributes
     **/
    class rorcfs_buffer
    {
    public:
      rorcfs_buffer();
      ~rorcfs_buffer();

        /**
        * Allocate buffer: This function initiates allocation of an
        * EventBuffer of [size] bytes with Buffer-ID [id]. The size
        * of the according ReportBuffer is determined by the driver.
        * @param dev pointer to parent rorcfs_device instance
        * @param size Size of EventBuffer in bytes
        * @param id Buffer-ID to be used for this buffer. This ID has to
        *        be unique within all instances of rorcfs_buffer on a machine.
        * @param overmap enables overmapping of the physical pages if nonzero
        * @param dma_direction select from RORCFS_DMA_FROM_DEVICE,
        *        RORCFS_DMA_TO_DEVICE, RORCFS_DMA_BIDIRECTIONAL
        * @return 0 on sucess, -1 on error
        **/
        int allocate
        (
          device* dev,
          unsigned long size,
          unsigned long id,
          int overmap,
          int dma_direction
        );

      /**
       * Free Buffer: This functions initiates de-allocation of the
       * attaced DMA buffers
       * @return 0 on sucess, <0 on error ( use perror() )
       **/
      int deallocate();

        /**
        * Connect to an existing buffer
        * @param dev parent rorcfs device
        * @param id buffer ID of exisiting buffer
        * @return 0 on sucessful connect, -EPERM or -ENOMEM on errors
        **/
        int
        connect
        (
          device* dev,
          unsigned long id
        );

      /**
       * get Buffer-ID
       * @return unsigned long Buffer-ID
       **/
      unsigned long getID() { return id; }

      /**
       * Get physical Buffer size in bytes. Requested buffer
       * size from init() is rounded up to the next PAGE_SIZE
       * boundary.
       * @return number of bytes allocated as Buffer
       **/
      unsigned long getPhysicalSize() { return PhysicalSize; }

      /**
       * Get size of the EB mapping. THis is double the size of
       * the physical buffer size due to overmapping
       * @return size of the EB mapping in bytes
       **/
      unsigned long getMappingSize() { return MappingSize; }

      /**
       * get the overmapped flag of the buffer
       * @return 0 if unset, nonzero if set
       **/
      int getOvermapped() { return overmapped; }

      /**
       * Get number of scatter-gather entries for the Buffer
       * @return (unsigned long) number of entries
       **/
      unsigned long getnSGEntries() { return nSGEntries; }

      /**
       * Get the maximum number of report buffer entries in the RB
       * @return maximum number of report buffer entries
       **/
      unsigned long getMaxRBEntries() {
        return (PhysicalSize / sizeof(struct rorcfs_event_descriptor));
      }

      /**
       * get sysfs directory name of the buffer
       * @return pointer to char string
       **/
      char* getDName() { return dname; }

      /**
       * get size of the dname string
       * @return size of the dname string in number of bytes
       **/
      int getDNameSize() { return dname_size; }

      /**
       * get memory buffer
       * @return pointer to mmap'ed buffer memory
       **/
      unsigned int* getMem() { return mem; }

    protected:
      // sysfs directory of buffer
      char* dname;

      // sysfs-mmap directory of driver
      char* base_name;

      int dname_size;
      int base_name_size;

      unsigned long id;
      unsigned long PhysicalSize;
      unsigned long MappingSize;
      unsigned long nSGEntries;
      int overmapped;
      int dma_direction;

      int fdEB;

      unsigned int* mem;
    };
}
#endif
