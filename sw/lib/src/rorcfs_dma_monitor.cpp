 /**
 * @file rorcfs_dma_monitor.cpp
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2011-09-26
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

#include <iostream>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>
#include <dirent.h>

#include "rorcfs.h"
#include "rorcfs_bar.hh"
#include "rorcfs_buffer.hh"
#include "rorcfs_dma_channel.hh"
#include "rorcfs_dma_monitor.hh"
#include "rorc_registers.h"

//#define MAX_PAYLOAD 128*1024

/**
 * scandir_filter:
 * return nonzero if a directory is found
 **/
static int scandir_filter(const struct dirent* entry) {
	if( entry->d_type == DT_DIR && strncmp(entry->d_name, ".", 1)!=0 )
		return 1;
	else
		return 0;
}


void *dma_monitor( void *buf )
{
	int n, *sockfd;
	unsigned char *buffer;
	int buffer_size = MAX_PAYLOAD + sizeof(unsigned long);
	char *basedir;
	int i, j, bufn, nentries, fd, nbytes, destfd;
	struct dirent **namelist;
	unsigned long addr, offset, wraplen;
	struct rorcfs_dma_desc desc;
	struct stat filestat, deststat;
	unsigned char *mem;
	int fname_size, dname_size;
	char *fname, *dname;
	int err;
	int dest_found = 0;
	unsigned char ack = 0x01;


	sockfd = (int *)buf;

	buffer = (unsigned char*)malloc(buffer_size);

	while( (n=read(*sockfd, buffer, buffer_size)) > 0 ) 
	{
		dest_found = 0;
		
		//printf("DMA_MON: Packet Received (%d bytes)\n", n);
		err = write(*sockfd, &ack, 1);
		if ( err<0 )
			printf("** DMA_MON: Error writing to socket\n");

		/**
		 * 64 bit address, then a variable number of data DWs, however
		 * max. MAX_PAYLOAD (unless there is no error)
		 **/

		//get start address
		memcpy(&addr, buffer, sizeof(addr));
		//printf("DMA_MON WR to %llx\n", addr);

		// find list of buffers
		rorcfs_device *dev = new rorcfs_device();	
		if ( dev->init(0) == -1 ) {
			printf("failed to initialize device 0\n");
		}
		dev->getDName(&basedir);

		bufn = scandir(basedir, &namelist, scandir_filter, alphasort);
		//printf("found %d buffers...\n", bufn);

		// iterate over all buffers
		for (i=0;i<bufn;i++) {


			// get filename
			fname_size = snprintf(NULL, 0, "%s%s/sglist", 
						basedir, namelist[i]->d_name) + 1;
			fname = (char *)malloc(fname_size);
			snprintf(fname, fname_size, "%s%s/sglist", 
						basedir, namelist[i]->d_name);

			//printf("scanning buffer %d: %s\n", i, fname);
			
			// get number of entries and read entries
			err = stat(fname, &filestat);
			if (err)
				printf("DMA_MON ERROR: failed to stat file %s\n", fname);
			nentries = filestat.st_size / sizeof( struct rorcfs_dma_desc);

			//printf("buffer has %d entries\n", nentries);

			fd = open(fname, O_RDONLY);
			if (fd==-1)
				perror("DMA_MON ERROR: open sglist");

			offset = 0;
			for(j=0; j<nentries; j++) {
				nbytes = read(fd, &desc, sizeof(struct rorcfs_dma_desc));
				if(nbytes!=sizeof(struct rorcfs_dma_desc))
					printf("DMA_MON ERROR: nbytes(=%d) != sizeof(struct "
							"rorcfs_dma_desc)\n",	nbytes);
				
				//printf("comparing dest addr %llx with segment %llx to %llx\n",
				//		addr, desc.addr, desc.addr+desc.len);

				// check if this is the destination segment
				if ( desc.addr <= addr && desc.addr + desc.len > addr )
				{
					//adjust offset
					offset += (addr - desc.addr);

					//get & malloc file name
					dname_size = snprintf(NULL, 0, "%s%s/mem", 
						basedir, namelist[i]->d_name) + 1;
					dname = (char *)malloc(dname_size);
					snprintf(dname, dname_size, "%s%s/mem", 
						basedir, namelist[i]->d_name);

					// open destination
					destfd = open(dname, O_RDWR);
					if (destfd==-1)
						perror("DMA_MON ERROR: open mem");


					if ( fstat(destfd, &deststat) == -1 )
						printf("DMA_MON ERROR: fstat failed");

					//printf("memcpy(%p, %p, %ld)\n", mem+offset, buffer+sizeof(addr),
					//		n-sizeof(addr));
					mem = (unsigned char*) mmap(0, deststat.st_size, 
							PROT_READ|PROT_WRITE, MAP_SHARED, destfd, 0);
					if ( mem == MAP_FAILED )
						printf("DMA_MON ERROR: mmap %s\n", dname);

					if( (offset + (unsigned long)n - sizeof(addr)) > 
							deststat.st_size ) 
					{
						// wrap at EOF
						wraplen = offset + n-sizeof(addr) - deststat.st_size;
						memcpy( mem+offset, buffer+sizeof(addr), wraplen);
						memcpy( mem, buffer+sizeof(addr)+wraplen, n-sizeof(addr)-wraplen );
#ifdef DEBUG
						printf("DBG_wrap: %ld to offset %lx\n", wraplen, offset);
						printf("DBG_wrap: %ld to offset 0\n", n-sizeof(addr)-wraplen);
#endif
					} else {
						memcpy(mem+offset, buffer+sizeof(addr), n-sizeof(addr));
#ifdef DEBUG
						//printf("DBG: %ld to offset %lx, first byte=%02x\n", 
						//		n - sizeof(addr), offset, (unsigned char)*(mem+offset));
#endif
					}

					dest_found = 1;

#ifdef DEBUG
					printf("DMAmon: %ld bytes to %s\n", n-sizeof(addr), 
							namelist[i]->d_name);
#endif

					munmap( mem, deststat.st_size );
					close(destfd);
					free(dname);
				} // dest-segment found
				offset += desc.len;
			} //iterate over segments
			free(fname);
			close(fd);

		} //iterate over buffers

		if( dest_found==0 ) {
			printf("DMA_MON ERROR: destination not found: %d bytes to addr=%lx\n", n, addr);
		}

	} //while(read)
	return NULL;
}



rorcfs_dma_monitor::rorcfs_dma_monitor()
{
	struct sockaddr_in serv_addr;
  struct hostent *server;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd < 0 )
		perror("ERROR opening socket");

	server = gethostbyname(MODELSIM_SERVER);

	if ( server == NULL ) 
		perror("ERROR, no sich host");

	bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(2001);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        perror("ERROR connecting");

		pthread_create(&dma_mon_p, NULL, &dma_monitor, &sockfd);
}

rorcfs_dma_monitor::~rorcfs_dma_monitor()
{
	pthread_cancel(dma_mon_p);
}


