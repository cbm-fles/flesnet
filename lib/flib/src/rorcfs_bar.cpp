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


