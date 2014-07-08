 /**
 * @file rorcfs_buffer.cpp
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
 * @section DESCRIPTION
 *
 */

#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>

#include <pda.h>

#include <flib/rorcfs_buffer.hh>

namespace flib
{


    rorcfs_buffer::rorcfs_buffer()
    {

    }

    rorcfs_buffer::~rorcfs_buffer()
    {
        munmap(m_mem, m_mapping_size);
        m_mem = NULL;
    }



    /**
     * Allocate Buffer: initiate memory allocation,
     * connect to new buffer & retrieve actual buffer sizes
     **/
    int rorcfs_buffer::allocate
    (
        device *dev,
        unsigned long  size,
        unsigned long  id,
        int            overmap,
        int            dma_direction
    )
    {
    //  char *fname;
    //  struct t_rorcfs_buffer buf;
    //  int fd, ret;
    //
    //  // already connected to another buffer? unmap first!
    //  if ( mem!=NULL ) {
    //      errno = EPERM;
    //      return -1;
    //  }
    //
    //  // get sysfs base directory name and size
    //  base_name_size = dev->getDName( &base_name );
    //
    //  fname = (char *) malloc( base_name_size + 12 );
    //  if (!fname) {
    //      errno = ENOMEM;
    //      return -1;
    //  }
    //
    //  snprintf(fname, base_name_size + 12 , "%salloc_buffer", base_name);
    //  buf.id = id;
    //  buf.bytes = size;
    //  buf.overmap = overmap;
    //  buf.dma_direction = dma_direction;
    //
    //  fd = open(fname, O_WRONLY);
    //  if ( fd==-1 ) {
    //      perror("open alloc_buffer");
    //      free(fname);
    //      return -1;
    //  }
    //
    //  ret = write( fd, &buf, sizeof(buf) );
    //  if ( ret != sizeof(buf) ) {
    //      //perror("write to alloc_buffer");
    //      close(fd);
    //      free(fname);
    //      return -1;
    //  }
    //
    //  free(fname);
    //  close(fd);
    //
    //  // connect to allocated buffer
    //  if( connect(dev, id) == -1 ) {
    //      return -1;
    //  }

        return 0;
    }



    /**
     * Release buffer
     **/
    int rorcfs_buffer::deallocate()
    {
        char *fname;
        int fd, ret;

        // not initialized!
        if(m_dname==NULL || m_dname_size==0 || m_physical_size==0)
            return -EINVAL;

        close(m_fdEB);

        fname = (char *) malloc( m_base_name_size + 11 );
        if (!fname)
            return -ENOMEM;

        snprintf(fname, m_base_name_size + 11 , "%sfree_buffer",	m_base_name);

        // open /sys/module/rorcfs/drivers/pci:rorcfs/[pci-ID]/mmap/free_buffer
        fd = open(fname, O_WRONLY);
        if ( fd==-1 ) {
            perror("open free_buffer");
            free(fname);
            return -1;
        }

        // write buffer-ID of buffer to be de-allocated
        ret = write( fd, &m_id, sizeof(m_id) );
        if ( ret != sizeof(m_id) ) {
            perror("write to free_buffer");
            free(fname);
            close(fd);
            return -1;
        }

        m_base_name = NULL;
        m_base_name_size = 0;
        m_physical_size = 0;
        m_mapping_size = 0;
        m_overmapped = 0;

        m_dname_size = 0;
        m_dname = NULL;

        free(fname);
        close(fd);
        return 0;
    }



    int
    rorcfs_buffer::connect
    (
        device *dev,
        unsigned long id
    )
    {
    //	char *fname;
    //	int fd, fname_size;
    //	struct stat filestat;
    //	int nbytes = 0;
    //
    //	if ( mem!=NULL ) { //already connected, unmap first!
    //		errno = EPERM;
    //		return -1;
    //	}
    //
    //	// get sysfs base directory name and size
    //	base_name_size = dev->getDName( &base_name );
    //
    //	// get MappingSize from sysfs attribute
    //	fname_size = snprintf(NULL, 0, "%s%03ld/mem", base_name, id);
    //	fname_size++;
    //	fname = (char *) malloc(fname_size);
    //	if (!fname) {
    //		errno = ENOMEM;
    //		return -1;
    //	}
    //
    //	snprintf(fname, fname_size, "%s%03ld/mem", base_name, id);
    //	//printf("fname=%s\n", fname);
    //	fdEB = open(fname, O_RDWR);
    //	if (fdEB==-1) {
    //		perror("open mem");
    //		free(fname);
    //		return -1;
    //	}
    //	free(fname);
    //
    //	if ( fstat(fdEB, &filestat) == -1 ) {
    //		close(fdEB);
    //		return -1;
    //	}
    //
    //	// set MappingSize to the size of the sysfs file
    //	MappingSize = filestat.st_size;
    //
    //	// calculate PhysicalSize with "overmapped" sysfs attribute
    //	fname_size = snprintf(NULL, 0, "%s%03ld/overmapped", base_name, id);
    //	fname_size++;
    //	fname = (char *) malloc(fname_size);
    //	if (!fname) {
    //		errno = ENOMEM;
    //		close(fdEB);
    //		return -1;
    //	}
    //
    //	snprintf(fname, fname_size, "%s%03ld/overmapped", base_name, id);
    //	fd = open(fname, O_RDONLY);
    //	if (fd==-1) {
    //		perror("open overmapped");
    //		free(fname);
    //		close(fdEB);
    //		return -1;
    //	}
    //	free(fname);
    //
    //	// read from "overmapped" - returns 1 or 0
    //	nbytes = read( fd, &overmapped, sizeof(int) );
    //	if( nbytes != sizeof(int) ) {
    //		perror("read overmapped");
    //		close(fd);
    //		close(fdEB);
    //		return -1;
    //	}
    //	close(fd);
    //
    //	// Set PhysicalSize attribute according to the contents of
    //	// the sysfs file "overmapped"
    //	if ( overmapped )
    //		PhysicalSize = MappingSize/2;
    //	else
    //		PhysicalSize = MappingSize;
    //
    //
    //
    //
    //	// MMap Buffer
    //	mem = (unsigned int*)mmap(0, MappingSize, PROT_READ|PROT_WRITE,
    //			MAP_SHARED, fdEB, 0);
    //	if( mem==MAP_FAILED ) {
    //		close(fdEB);
    //		perror("mmap mem");
    //		return -1;
    //	}
    //
    //	// get nSGEntries from sysfs attribute
    //	fname_size = snprintf(NULL, 0, "%s%03ld/sglist", base_name, id);
    //	fname_size++;
    //	fname = (char *) malloc(fname_size);
    //	if (!fname) {
    //		errno = ENOMEM;
    //		return -1;
    //	}
    //
    //	snprintf(fname, fname_size, "%s%03ld/sglist", base_name, id);
    //	fd = open(fname, O_RDONLY);
    //	if (fd==-1) {
    //		free(fname);
    //		perror("open sglist");
    //		return -1;
    //	}
    //	free(fname);
    //
    //	if ( fstat(fd, &filestat) == -1 ) {
    //		close(fd);
    //		return -1;
    //	}
    //
    //	nSGEntries = filestat.st_size / sizeof(struct rorcfs_dma_desc);
    //
    //	close(fd);
    //
    //	// store buffer id
    //	this->id = id;
    //
    //	// save sysfs directory name of created buffer
    //	// e.g. /sys/module/rorcfs/drivers/pci:rorcfs/0000:03:00.0/mmap/001/
    //	dname_size = snprintf(NULL, 0, "%s%03ld/", base_name, id);
    //	dname_size++;
    //	dname = (char *) malloc(dname_size);
    //	if (!dname) {
    //		errno = ENOMEM;
    //		return -1;
    //	}
    //
    //	snprintf(dname, dname_size, "%s%03ld/", base_name, id);
    //
    //	librorc_debug("librorc::connect ID=%ld, PhysSize=%ld, MapSize=%ld, "
    //			"nSG=%ld, overmapped=%d, dma_direction=%d\n",
    //			id, PhysicalSize, MappingSize, nSGEntries, overmapped,
    //			dma_direction );
    //
        return 0;
    }

}
