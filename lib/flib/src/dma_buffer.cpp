#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>


#include <iostream>
#include <pda.h>

#include <flib/dma_buffer.hpp>
#include <flib/device.hpp>

using namespace std;

namespace flib
{


    dma_buffer::dma_buffer()
    {

    }

    dma_buffer::~dma_buffer()
    {
        munmap(m_mem, m_mapping_size);
        m_mem = NULL;
    }



    /**
     * Allocate Buffer: initiate memory allocation,
     * connect to new buffer & retrieve actual buffer sizes
     **/
    int
    dma_buffer::allocate
    (
        device   *device,
        uint64_t  size,
        uint64_t  id,
        int       overmap,
        int       dma_direction
    )
    {
        m_dma_direction = dma_direction;
        m_device        = device->m_device;
        m_id            = id;

        if
        (
            PDA_SUCCESS !=
                PciDevice_allocDMABuffer
                    (m_device, id, size, PDABUFFER_DIRECTION_BI, &m_buffer)
        )
        { return -1; }

        if(overmap == 1)
        {
            if(PDA_SUCCESS != DMABuffer_wrapMap(m_buffer) )
            { return -1; }
        }


        return 0;
    }



    /**
     * Release buffer
     **/
    int dma_buffer::deallocate()
    {
        if(DMABuffer_free(m_buffer, PDA_DELETE) != PDA_SUCCESS)
        { return -1; }

        m_id = 0;
        m_dma_direction = 0;

        return 0;
    }



    int
    dma_buffer::connect
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
