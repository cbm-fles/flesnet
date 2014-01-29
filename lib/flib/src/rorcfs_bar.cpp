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

#ifdef SIM
	struct sockaddr_in serv_addr;
	struct hostent *server;
	//struct in_addr ipaddr;
	int statusFlags;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd < 0 )
		perror("ERROR opening socket");

	server = gethostbyname(MODELSIM_SERVER);
	//inet_pton(AF_INET, "10.0.52.10", &ipaddr);
	//server = gethostbyaddr(&ipaddr, sizeof(ipaddr), AF_INET);
	if ( server == NULL ) 
		perror("ERROR, no sich host");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, 
			(char *)&serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(2000);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,
				sizeof(serv_addr)) < 0) 
		perror("ERROR connecting");

	statusFlags = fcntl(sockfd, F_GETFL );
	if ( statusFlags == -1 ) {
		perror( "Getting socket status" );
	} else {
		int ctlValue;
		statusFlags |= O_NONBLOCK;
		ctlValue = fcntl( sockfd, F_SETFL, statusFlags );
		if (ctlValue == -1 ) {
			perror("Setting socket status");
		}
	}
	msgid = 0;

#endif
}

rorcfs_bar::~rorcfs_bar()
{
#ifdef SIM
	close(sockfd);
#endif
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
#ifndef SIM
	handle = open(fname, O_RDWR);
	if ( handle == -1 )
		return handle;

	if ( fstat(handle, &barstat) == -1 )
		return -1;

	bar = (unsigned int*) mmap(0, barstat.st_size, 
			PROT_READ|PROT_WRITE, MAP_SHARED, handle, 0);
	if ( bar == MAP_FAILED )
		return -1;
#endif
	return 0;
}

unsigned int rorcfs_bar::get(unsigned long addr) {
#ifndef SIM
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
#else

	pthread_mutex_lock(&mtx);
	
	flicmdT cmd;
	flimsgT msg;
	int n;	
	
	cmd.type= CMD_READ;
	cmd.addr = addr<<2;
	cmd.bar = number;
	cmd.byte_enable = 0x0f;
	cmd.id = msgid;

	n = write(sockfd, &cmd, sizeof(cmd));
	if ( n < 0 )
		std::cout << "ERROR writing to socket: " << n << "\n";
	
	// wait for completion
	while( read( sockfd, &msg, sizeof(msg)) != sizeof(msg) )
		usleep(USLEEP_TIME);

#ifdef DEBUG
	printf("%d: rorcfs_bar::get(%lx)=%08x %s\n", 
			msg.id, addr, msg.data, getChOff(addr));
	fflush(stdout);
#endif

	if ( msg.id!=msgid ) {
		printf("ERROR: out of order message ID: "
				"rorcfs_bar::get(%lx)=%08x\n", addr, msg.data);
	}

	msgid++;
	pthread_mutex_unlock(&mtx);
	return msg.data;
#endif
}

void rorcfs_bar::set(unsigned long addr, unsigned int data)
{

#ifndef SIM
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

#else
	// semd write command to Modelsim FLI server
	flicmdT cmd;
	flimsgT msg;
	unsigned int *buffer;
	int n, buffersize;
	unsigned int data32;

	pthread_mutex_lock(&mtx);

	cmd.type = CMD_WRITE;
	cmd.addr = addr<<2;
	cmd.bar = number;
	cmd.byte_enable = 0x0f;
	cmd.id = msgid;
	cmd.len = 1;
	data32 = data;

	buffersize = sizeof(flicmdT)+sizeof(unsigned int);
	buffer = (unsigned int *)malloc(buffersize);
	memcpy(buffer, &cmd, sizeof(flicmdT));
	memcpy((unsigned char *)(buffer) + sizeof(flicmdT), &data32, sizeof(unsigned int));

	n = write(sockfd, buffer, buffersize);
	if ( n < 0 )
		std::cout << "ERROR writing to socket: " << n << "\n";
	// wait for completion
	while( read( sockfd, &msg, sizeof(msg)) != sizeof(msg) )
		usleep(USLEEP_TIME);
#ifdef DEBUG
	printf("%d: rorcfs_bar::set(%lx, %08x) %s\n", 
			msg.id, addr, data, getChOff(addr));
	fflush(stdout);
#endif

	if ( msg.id!=msgid ) {
		printf("ERROR: out of order message ID: "
				"rorcfs_bar::set(%lx)=%08x\n", addr, msg.data);
	}
	msgid++;
	free(buffer);
	pthread_mutex_unlock(&mtx);

#endif
}


void rorcfs_bar::memcpy_bar(unsigned long addr, const void *source, size_t num) {
#ifndef SIM
#ifdef DEBUG
	printf("rorcfs_bar:memcpy_bar(%lx, %p, %ld)\n", addr, source, num);
#endif
	pthread_mutex_lock(&mtx);
	memcpy((unsigned char *)bar + (addr<<2), source, num);
	msync( (bar + ((addr<<2) & PAGE_MASK)), PAGE_SIZE, MS_SYNC);
	pthread_mutex_unlock(&mtx);
#else
	
	flicmdT cmd;
	flimsgT msg;
	unsigned int *buffer;
	int n, buffersize;
	
	pthread_mutex_lock(&mtx);

	cmd.type = CMD_WRITE;
	cmd.addr = addr<<2;
	cmd.bar = number;
	if(num>1)
		cmd.byte_enable = 0xff;
	else
		cmd.byte_enable = 0x0f;
	cmd.id = msgid;
	cmd.len = num>>2;
	
	buffersize = sizeof(flicmdT)+num;
	buffer = (unsigned int *)malloc(buffersize);
	memcpy(buffer, &cmd, sizeof(flicmdT));
	memcpy((unsigned char *)(buffer) + sizeof(flicmdT), source, num);

	n = write(sockfd, buffer, buffersize);
	if ( n < 0 )
		std::cout << "ERROR writing to socket: " << n << "\n";
	// wait for completion
	while( read( sockfd, &msg, sizeof(msg)) != sizeof(msg) )
		usleep(USLEEP_TIME);
#ifdef DEBUG
	printf("%d: rorcfs_bar::memcpy(%lx, %d DWs) %s\n", 
			msg.id, addr, cmd.len, getChOff(addr));
	fflush(stdout);
#endif

	if ( msg.id!=msgid ) {
		printf("ERROR: out of order message ID: "
				"rorcfs_bar::memcpy(%lx)=%08x\n", addr, msg.data);
	}
	msgid++;
	free(buffer);
	pthread_mutex_unlock(&mtx);

#endif
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

unsigned short rorcfs_bar::get16(unsigned long addr) 
{
#ifndef SIM
	unsigned short *sbar;
	sbar = (unsigned short *)bar;
	unsigned short result;
	assert ( sbar!=NULL );
	if ( (addr<<1)<(unsigned long)barstat.st_size ) {
		result = sbar[addr];
#ifdef DEBUG
		printf("rorcfs_bar::get16(%lx)=%04x\n", addr, result);
		fflush(stdout);
#endif

		return result;
	}
	else
		return 0xffff;
#else

	pthread_mutex_lock(&mtx);

	flicmdT cmd;
	flimsgT msg;
	int n;	

	cmd.type = CMD_READ;
	cmd.addr = (addr<<1) & ~(0x03);
	cmd.bar = number;
	cmd.id = msgid;

	if ( addr & 0x01 )
		cmd.byte_enable = 0x0c;
	else
		cmd.byte_enable = 0x03;

	n = write(sockfd, &cmd, sizeof(cmd));
	if ( n < 0 )
		std::cout << "ERROR writing to socket: " << n << "\n";
	
	// wait for completion
	while( read( sockfd, &msg, sizeof(msg)) != sizeof(msg) )
		usleep(USLEEP_TIME);

#ifdef DEBUG
	printf("rorcfs_bar::get16(%lx)=%04x\n", addr, 
			(unsigned short)(msg.data & 0x0000ffff) );
	fflush(stdout);
#endif
	
	if ( msg.id!=msgid ) {
		printf("ERROR: out of order message ID: "
				"rorcfs_bar::get16(%lx)=%08x\n", addr, msg.data);
	}

	msgid++;

	pthread_mutex_unlock(&mtx);

	return (unsigned short)(msg.data & 0x0000ffff);
#endif
}



void rorcfs_bar::set16(unsigned long addr, unsigned short data) 
{
#ifndef SIM	
	unsigned short *sbar;
	sbar = (unsigned short *)bar;
	// access mmap'ed BAR region
	assert ( sbar!=NULL );
	if ( (addr<<1)<(unsigned long)barstat.st_size ) {
		pthread_mutex_lock(&mtx);
		sbar[addr] = data;
		msync( (sbar + ((addr<<1) & PAGE_MASK)), 
			PAGE_SIZE, MS_SYNC);
		pthread_mutex_unlock(&mtx);
#ifdef DEBUG
		printf("rorcfs_bar::set16(%lx, %04x)\n", addr, data);
		fflush(stdout);
#endif
	}

#else
	
	flicmdT cmd;
	flimsgT msg;
	int n;
	unsigned int data32;
	unsigned int *buffer;

	pthread_mutex_lock(&mtx);
	
	cmd.type = CMD_WRITE;
	cmd.addr = (addr<<1) & ~(0x03);
	cmd.bar = number;
	//cmd.data = (data<<16) + data;
	data32 = (data<<16) + data;
	cmd.id = msgid;

	if ( addr & 0x01 )
		cmd.byte_enable = 0x0c;
	else
		cmd.byte_enable = 0x03;

	buffer = (unsigned int *)malloc(sizeof(cmd)+sizeof(unsigned int));
	memcpy(buffer, &cmd, sizeof(cmd));
	memcpy((unsigned char *)(buffer) + sizeof(flicmdT), 
			&data32, sizeof(unsigned int));

	n = write(sockfd, &cmd, sizeof(cmd));
	if ( n < 0 )
		std::cout << "ERROR writing to socket: " << n << "\n";
	
	// wait for completion
	while( read( sockfd, &msg, sizeof(msg)) != sizeof(msg) )
		usleep(USLEEP_TIME);

#ifdef DEBUG
	printf("rorcfs_bar::set16(%lx)=%04x\n", addr, data );
	fflush(stdout);
#endif

	if ( msg.id!=msgid ) {
		printf("ERROR: out of order message ID: "
				"rorcfs_bar::set16(%lx)=%08x\n", addr, msg.data);
	}
	msgid++;

	pthread_mutex_unlock(&mtx);

#endif
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
	


