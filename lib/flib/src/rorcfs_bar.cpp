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

rorcfs_bar::rorcfs_bar
(
    device *dev,
    uint8_t number
)
{
    m_parent_dev     = dev;
    m_number         = number;
//    m_pda_pci_device = m_parent_dev->getPdaPciDevice();
//    m_bar            = m_parent_dev->getBarMap(m_number);
//    m_size           = m_parent_dev->getBarSize(m_number);

    if(m_bar == NULL)
    { throw BAR_ERROR_CONSTRUCTOR_FAILED; }

    pthread_mutex_init(&m_mtx, NULL);
}

rorcfs_bar::~rorcfs_bar()
{
//	pthread_mutex_destroy(&mtx);
//	if (fname)
//		free(fname);
//	if (bar)
//		munmap( bar, barstat.st_size);
//	if (handle)
//		close(handle);
}
