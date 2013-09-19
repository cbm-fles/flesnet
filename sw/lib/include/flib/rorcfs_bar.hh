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
class rorcfs_bar
{
	public:

		/**
		 * Constructor that sets fname accordingly. No mapping is 
		 * performed at this point.
		 * @param dev parent rorcfs_device
		 * @param n number of BAR to be mapped [0-6]
		 **/
		rorcfs_bar(rorcfs_device *dev, int n);

		/**
		 * Deconstructor: free fname, unmap BAR, close file
		 **/
		~rorcfs_bar();

		/**
		 * read DWORD from BAR address
		 * @param addr (unsigned int) aligned address within the 
		 * 				BAR to read from.
		 * @return data read from BAR[addr]
		 **/
		unsigned int get( unsigned long addr );

		/**
		 * read WORD from BAR address
		 * @param addr within the BAR to read from.
		 * @return data read from BAR[addr]
		 **/
		unsigned short get16( unsigned long addr );

		/**
		 * copy buffer range into BAR
		 * @param addr address in current BAR
		 * @param source pointer to source data field
		 * @param num number of bytes to be copied to destination
		 * */
		void memcpy_bar( unsigned long addr, const void *source, size_t num );
		
                /**
		 * copy buffer range into BAR
		 * @param addr address in current BAR
		 * @param source pointer to source data field
		 * @param n number of bytes to be copied to BAR, must be multible of 4
		 * */
		void set_mem( unsigned long addr, const void *source, size_t n );
		
                /**
		 * copy buffer range from BAR
		 * @param addr address in current BAR
		 * @param dest pointer to destination data field
		 * @param n number of bytes to be copied to destination, must be multible of 4
		 * */
                void get_mem( unsigned long addr, void *dest, size_t n );
		
                /**
		 * write DWORD to BAR address
		 * @param addr (unsigned int) aligned address within the 
		 * 				BAR to write to
		 * @param data (unsigned int) data word to be written.
		 **/
		void set( unsigned long addr, unsigned int data );

  /**
   * set or unset single bit in register at address
   * @param addr aligned address within the 
   * 				BAR to write to
   * @param pos bit possition to modify
   * @param enable true enables the bit
   **/
  void set_bit(uint64_t addr, int pos, bool enable);

		/**
		 * write WORD to BAR address
		 * @param addr within the BAR to write to
		 * @param data (unsigned int) data word to be written.
		 **/
		void set16( unsigned long addr, unsigned short data );

		/**
		 * get current time of day
		 * @param tv pointer to struct timeval
		 * @param tz pointer to struct timezone
		 * @return return valiue from gettimeof day or zero for FLI simulation
		 **/
		int gettime(struct timeval *tv, struct timezone *tz);

		/**
		 * get file handle for sysfs file
		 * @return int file handle
		 **/
		int getHandle() { return handle; };

		/**
		 * initialize BAR mapping: open sysfs file, get file stats, 
		 * mmap file. This has to be done before using any other 
		 * member funtion. This function will fail if the requested 
		 * BAR does not exist.
		 * @return 0 on sucess, -1 on errors
		 **/
		int init();

		/**
		 * get size of mapped BAR. This value is only valid after init()
		 * @return size of mapped BAR in (unsigned long) bytes
		 **/
		unsigned long getSize() { return barstat.st_size; }

	private:
		int handle;
		char *fname;
		struct stat barstat;
		unsigned int *bar;
		int number;
		pthread_mutex_t mtx;
#ifdef SIM
		int sockfd;
		int msgid;
#endif
};

#ifdef SIM
typedef struct {
	int wr_ack;
	unsigned int data;
	int id;
} flimsgT;

typedef struct {
	unsigned int addr;
	unsigned int bar;
	unsigned char byte_enable;
	unsigned char type;
	unsigned short len;
	int id;
} flicmdT;

/** bit encodings for command type **/
#define CMD_READ (1<<0)
#define CMD_WRITE (1<<1)
#define CMD_TIME (1<<2)
#endif

#endif
