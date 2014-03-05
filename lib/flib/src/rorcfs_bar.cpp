/**
 * @file rorcfs_bar.cpp
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
 */

#include <iostream>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdint.h>

#include "../include/flib/rorcfs.h"
#include "../include/flib/rorcfs_bar.hh"
#include "../include/flib/rorcfs_device.hh"
#include "../include/flib/rorc_registers.h"

/**
 * usleep time for FLI read polling
 * */
#define USLEEP_TIME 50

/** getChOff: 
 * @return a string describing the register
 * being accessed
 * @param addr address in FPGA
 **/
char *getChOff(unsigned int addr);


// TODO:
// remove all unused functions

rorcfs_bar::rorcfs_bar(rorcfs_device *dev, int n)
{
	char basedir[] = "/sys/module/rorcfs/drivers/pci:rorcfs/";
	char subdir[] = "/mmap/";
	int digits = snprintf(NULL, 0, "%d", n);


	bar = NULL;

	fname = (char *) malloc(strlen(basedir) + 12	+ 
			strlen(subdir) + 3 + digits +1);

	// get sysfs file name for selected bar
	sprintf(fname, "%s0000:%02x:%02x.%x%sbar%d", 
			basedir, dev->getBus(),
			dev->getSlot(), dev->getFunc(), subdir, n);

	number = n;
	
	// initialize mutex
	pthread_mutex_init(&mtx, NULL);
}

rorcfs_bar::~rorcfs_bar()
{
	pthread_mutex_destroy(&mtx);
	if (fname)
		free(fname);
	if (bar)
		munmap( bar, barstat.st_size);
	if (handle)
		close(handle);
}

int rorcfs_bar::init()
{
	handle = open(fname, O_RDWR);
	if ( handle == -1 )
		return handle;

	if ( fstat(handle, &barstat) == -1 )
		return -1;

	bar = (unsigned int*) mmap(0, barstat.st_size, 
			PROT_READ|PROT_WRITE, MAP_SHARED, handle, 0);
	if ( bar == MAP_FAILED ) {
          return -1;
        }

	return 0;
}

unsigned int rorcfs_bar::get(unsigned long addr) {
	unsigned int result;
	assert ( bar!=NULL );
	if ( (addr<<2)<(unsigned long)barstat.st_size ) {
		result = bar[addr];
#ifdef DEBUG
	printf("rorcfs_bar::get(%lx)=%08x\n", addr, result);
	fflush(stdout);
#endif

	return result;
	} else
		return -1;
}

void rorcfs_bar::set(unsigned long addr, unsigned int data)
{
	// access mmap'ed BAR region
	assert ( bar!=NULL );
	if ( (addr<<2)<(unsigned long)barstat.st_size ) {
		pthread_mutex_lock(&mtx);
		bar[addr] = data;
		msync( (bar + ((addr<<2) & PAGE_MASK)), PAGE_SIZE, MS_SYNC);
		pthread_mutex_unlock(&mtx);
#ifdef DEBUG
		printf("rorcfs_bar::set(%lx, %08x)\n", addr, data);
		fflush(stdout);
#endif
	}
}


void rorcfs_bar::memcpy_bar(unsigned long addr, const void *source, size_t num) {
#ifdef DEBUG
	printf("rorcfs_bar:memcpy_bar(%lx, %p, %ld)\n", addr, source, num);
#endif
	pthread_mutex_lock(&mtx);
	memcpy((unsigned char *)bar + (addr<<2), source, num);
	msync( (bar + ((addr<<2) & PAGE_MASK)), PAGE_SIZE, MS_SYNC);
	pthread_mutex_unlock(&mtx);
}

// TODO basic functions should be able to report error
void rorcfs_bar::set_mem(unsigned long addr, const void *source, size_t n) {
  // Size n (in bytes) must be devidable by 32 bit
  assert ( bar!=NULL );
  assert ( n%4 == 0 );
  size_t words = n>>2;
  if ( (addr<<2)<(unsigned long)barstat.st_size ) {
    pthread_mutex_lock(&mtx);
    for (size_t i = 0; i < words; i++) {
      bar[addr+i] = *((uint32_t*)source+i);
      msync( (bar + (((addr+i)<<2) & PAGE_MASK)), PAGE_SIZE, MS_SYNC);
#ifdef DEBUG
    printf("rorcfs_bar::set_mem(%lx, %08x)\n", addr+i, *((uint32_t*)source + i));
    fflush(stdout);
#endif
    }
    pthread_mutex_unlock(&mtx);
  }
}
void rorcfs_bar::get_mem(unsigned long addr, void *dest, size_t n) {
  assert ( bar!=NULL );
  assert ( n%4 == 0 );
  size_t words = n>>2;
  if ( (addr<<2)<(unsigned long)barstat.st_size ) {
    for (size_t i = 0; i < words; i++) {
      *((uint32_t*)dest+i) = bar[addr+i];
#ifdef DEBUG
      printf("rorcfs_bar::get_mem(%lx)=%08x\n", addr+i, *((uint32_t*)dest+i));
      fflush(stdout);
#endif
    }
  }
}

void rorcfs_bar::set_bit(uint64_t addr, int pos, bool enable) {
  uint32_t reg= get(addr);
  if ( enable ) {
    set(addr, (reg | (1<<pos)));
  }
  else {
    set(addr, (reg & ~(1<<pos)));
  }    
}


char *getChOff(unsigned int addr) {
	int channel = (addr>>15)&0xf;
	int comp = (addr>>RORC_DMA_CMP_SEL) & 0x01;
	int offset = addr & ((1<<RORC_DMA_CMP_SEL)-1);
	char *buffer = (char *)malloc(256);
	sprintf(buffer, "ch:%d comp:%d off:%d", 
			channel, comp, offset);
	return buffer;
}
	


