/**
 * @file rorcfs_device.hh
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

/**
 * @mainpage
 * @author Heiko Engel <hengel@cern.ch>
 * @date 2011-08-16
 * @brief Library to interface to rorcfs kernel driver
 *
 * \section Organization
 * A rorcfs_device object corresponds to a phsical RORC, resp. a PCI 
 * device controlled by the rorcfs device driver. Use rorcfs_device::init()
 * to bind the object to the hardware.
 *
 * Objects of rorcfs_bar can be attached to a rorcfs_device to control
 * its PCIe Base Address Registers (BARs). Once the rorcfs_bar object is
 * initialized with rorcfs_bar::init() it can perform low level IO to/from 
 * the RORC via rorcfs_bar::get() or rorcfs_bar::set() methods. These 
 * methods are widely used in the rorcLib.
 *
 * A rorcfs_buffer object represents a DMA buffer on the host machine. This
 * buffers are mapped to the RORC for DMA transfers. Buffers can be 
 * allocated and deallocated with rorcfs_buffer::allocate() and 
 * rorcfs_buffer::deallocate(). The object can be bound to existing buffers
 * via the rorcfs_buffer::connect() method.
 *
 * All DMA operations are handled with objects of rorcfs_dma_channel. These 
 * objects have to be initialized to a specific bar and offset within the
 * bar with rorcfs_dma_channel::init(). After initialization the DMA buffer
 * information can be copied to the RORC, the maximum payload can be set and
 * the DMA channel can be enabled. Once data is available it is written to the
 * buffers on the host machine.
 *
 * In order to receive data a DIU has to be instantiated. This is done with an
 * object of rorcfs_diu. The data source of the rorcfs_diu object can either
 * be an actual DIU or a pattern generator at the DIU Interface. The data source
 * can be selected with rorcfs_diu::setDataSource(). If a pattern generator is
 * chosen, an according rorcfs_pattern_generator object has to be attached to the 
 * DIU in order to control the pattern generator parameters.
 **/

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
#define librorc_debug(fmt, args...) printf(fmt, ## args)
#else
#define librorc_debug(fmt, args...)
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
class rorcfs_device 
{
	public:
		rorcfs_device();
		~rorcfs_device();

		/**
		 * Initialize device. This has to be done before using
		 * any other member funtion.
		 * @param n device-number to be used. n is the n-th directory
		 * 					in /sys/module/rorcfs/drivers/pci:rorcfs/ with name
		 * 					0000:[Bus-ID]:[Slot-ID].[Function-ID] as returned by
		 * 					scandir() with alphasort().
		 * @return 0 on sucess, -1 on error or if device
		 * 				 does not exist
		 **/
		int init(int n=0);

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

		/**
		 * get SysFS directory name
		 * @param ptr char** pointer for the directory name
		 * @return size of directory name
		 **/
		int getDName( char **ptr );

	private:
		char *dname;
		int dname_size;
		uint8 bus;
		uint8 slot;
		uint8 func;

	protected:
		int find_rorc(char *basedir, struct dirent ***namelist);
};

#endif
