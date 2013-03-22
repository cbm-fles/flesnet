/**
 * @file rorcfs_dma_channel.cpp
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

#include <iostream>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>

#include "rorcfs_bar.hh"
#include "rorcfs_buffer.hh"
#include "rorc_registers.h"
#include "rorcfs_dma_channel.hh"

/** extern error number **/
extern int errno;

/*
 * Constructor
 * */
rorcfs_dma_channel::rorcfs_dma_channel()
{
	base = 0;
	bar = NULL;
	cMaxPayload = 0;
}


/**
 * Desctructor
 * */
rorcfs_dma_channel::~rorcfs_dma_channel()
{
	base = 0;
	bar = NULL;
	cMaxPayload = 0;
}


/**
 * Initialize Channel:
 * bind to specific BAR, set offset address for register access
 * */
void rorcfs_dma_channel::init(rorcfs_bar *dma_bar, unsigned int dma_base)
{
	base = dma_base;
	bar = dma_bar;
}


/**
 * prepareRB
 * Fill ReportBufferDescriptorRAM with scatter-gather
 * entries of DMA buffer
 * */
int rorcfs_dma_channel::prepareEB ( rorcfs_buffer *buf )
{
	char *fname;
	int fd, nbytes, ret = 0;
	unsigned int bdcfg;
	unsigned long i;
	struct rorcfs_dma_desc dma_desc;
	struct t_sg_entry_cfg sg_entry;

	assert( bar!=NULL );

	//open buf->mem_sglist
	fname = (char *) malloc(buf->getDNameSize() + 6);
	snprintf(fname, buf->getDNameSize() + 6, "%ssglist",
			buf->getDName());
	fd = open(fname, O_RDONLY);
	if (fd==-1) {
		free(fname);
		return -1;
	}
	free(fname);

	/**
	 * get maximum number of sg-entries supported
	 * by the firmware
	 * N_SG_CONFIG:
	 * [15:0] : actual number of sg entries in RAM
	 * [31:16]: maximum number of entries
	 * */
	bdcfg = getPKT( RORC_REG_EBDM_N_SG_CONFIG );

	// check if buffers SGList fits into EBDRAM
	if(buf->getnSGEntries() > (bdcfg>>16) ) {
		ret = -EFBIG;
		errno = EFBIG;
		goto close_fd;
	}

	// fetch all sg-entries from sglist
	for(i=0;i<buf->getnSGEntries();i++) {
		//read multiples of struct rorcfs_dma_desc
		nbytes = read(fd, &dma_desc, sizeof(struct rorcfs_dma_desc));
		if(nbytes!=sizeof(struct rorcfs_dma_desc)) {
			ret = -EBUSY;
			perror("prepareEB:read(rorcfs_dma_desc)");
			goto close_fd;
		}
		sg_entry.sg_addr_low = (uint32_t)(dma_desc.addr & 0xffffffff);
		sg_entry.sg_addr_high = (uint32_t)(dma_desc.addr >> 32);
		sg_entry.sg_len = (uint32_t)(dma_desc.len);
		sg_entry.ctrl = (1<<31) | (0<<30) | ((uint32_t)i);

		//write rorcfs_dma_desc to RORC EBDM
		bar->memcpy_bar(base + RORC_REG_SGENTRY_ADDR_LOW, 
				&sg_entry, sizeof(sg_entry));
	}

	// clear following BD entry (required!)
	memset(&sg_entry, 0, sizeof(sg_entry));
	bar->memcpy_bar(base + RORC_REG_SGENTRY_ADDR_LOW, 
			&sg_entry, sizeof(sg_entry));

close_fd:
	close(fd);
	return ret;
}


/**
 * configure DMA engine for the current
 * set of buffers
 * */
int rorcfs_dma_channel::configureChannel(
		struct rorcfs_buffer *ebuf,
		struct rorcfs_buffer *rbuf,
		uint32_t max_payload)
{
	struct rorcfs_channel_config config;

	// MAX_PAYLOAD has to be provided as #DWs
	// -> divide size by 4
	uint32_t mp_size = max_payload>>2;

	// N_SG_CONFIG:
	// [15:0] : actual number of sg entries in RAM
	// [31:16]: maximum number of entries
	uint32_t rbdmnsgcfg = getPKT( RORC_REG_RBDM_N_SG_CONFIG );
	uint32_t ebdmnsgcfg = getPKT( RORC_REG_RBDM_N_SG_CONFIG );

	// check if sglist fits into FPGA buffers
	if ( ((rbdmnsgcfg>>16) < rbuf->getnSGEntries()) | 
			((ebdmnsgcfg>>16) < ebuf->getnSGEntries())) {
		errno = -EFBIG;
		return errno;
	}

	if ( max_payload & 0x3 ) {
		// max_payload must be a multiple of 4 byte
		errno = -EINVAL;
		return errno;
	}	else if (max_payload>1024) {
		errno = -ERANGE;
		return errno;
	}

	config.ebdm_n_sg_config = ebuf->getnSGEntries();
	config.ebdm_buffer_size_low = (ebuf->getPhysicalSize()) & 0xffffffff;
	config.ebdm_buffer_size_high = ebuf->getPhysicalSize() >> 32;
	config.rbdm_n_sg_config = rbuf->getnSGEntries();
	config.rbdm_buffer_size_low = rbuf->getPhysicalSize() & 0xffffffff;
	config.rbdm_buffer_size_high = rbuf->getPhysicalSize() >> 32;

	config.swptrs.ebdm_software_read_pointer_low = 
		(ebuf->getPhysicalSize() - max_payload) & 0xffffffff;
	config.swptrs.ebdm_software_read_pointer_high = 
		(ebuf->getPhysicalSize() - max_payload) >> 32;
	config.swptrs.rbdm_software_read_pointer_low = 
		(rbuf->getPhysicalSize() - sizeof(struct rorcfs_event_descriptor)) &
		0xffffffff;
	config.swptrs.rbdm_software_read_pointer_high = 
		(rbuf->getPhysicalSize() - sizeof(struct rorcfs_event_descriptor)) >> 32;
		
	// set new MAX_PAYLOAD size
	config.swptrs.dma_ctrl = 
		(1<<31) | // sync software read pointers
		(mp_size<<16); // set max_payload

	// copy configuration struct to RORC, starting
	// at the address of the lowest register(EBDM_N_SG_CONFIG)
	bar->memcpy_bar(base + RORC_REG_EBDM_N_SG_CONFIG, &config,
			sizeof(struct rorcfs_channel_config));
	cMaxPayload = max_payload;

	return 0;
}

void rorcfs_dma_channel::setEnableEB( int enable )
{
	unsigned int bdcfg = getPKT( RORC_REG_DMA_CTRL );
	if ( enable )
		setPKT(RORC_REG_DMA_CTRL, ( bdcfg | (1<<2) ) );
	else
		setPKT(RORC_REG_DMA_CTRL, ( bdcfg & ~(1<<2) ) );
}

unsigned int rorcfs_dma_channel::getEnableEB()
{
	return (getPKT( RORC_REG_DMA_CTRL ) >> 2 ) & 0x01;
}

int rorcfs_dma_channel::prepareRB ( rorcfs_buffer *buf )
{
	char *fname;
	int fd, nbytes, ret = 0;
	unsigned int bdcfg;
	unsigned long i;
	struct rorcfs_dma_desc dma_desc;
	struct t_sg_entry_cfg sg_entry;

	assert( bar!=NULL );

	//open buf->mem_sglist
	fname = (char *) malloc(buf->getDNameSize() + 6);
	snprintf(fname, buf->getDNameSize() + 6, "%ssglist",
			buf->getDName());
	fd = open(fname, O_RDONLY);
	if (fd==-1) {
		free(fname);
		return -1;
	}
	free(fname);

	// N_SG_CONFIG:
	// [15:0] : actual number of sg entries in RAM
	// [31:16]: maximum number of entries
	bdcfg = getPKT( RORC_REG_RBDM_N_SG_CONFIG );

	// check if buffers SGList fits into RBDRAM
	if(buf->getnSGEntries() > (bdcfg>>16) ) {
		ret = -EFBIG;
		errno = EFBIG;
		goto close_fd;
	}

	for(i=0;i<buf->getnSGEntries();i++) {
		//read multiples of struct rorcfs_dma_desc
		nbytes = read(fd, &dma_desc, sizeof(struct rorcfs_dma_desc));
		if(nbytes!=sizeof(struct rorcfs_dma_desc)) {
			ret = -EBUSY;
			perror("prepareRB:read(rorcfs_dma_desc)");
			goto close_fd;
		}
		sg_entry.sg_addr_low = (uint32_t)(dma_desc.addr & 0xffffffff);
		sg_entry.sg_addr_high = (uint32_t)(dma_desc.addr >> 32);
		sg_entry.sg_len = (uint32_t)(dma_desc.len);
		sg_entry.ctrl = (1<<31) | (1<<30) | ((uint32_t)i);

		//write rorcfs_dma_desc to RORC EBDM
		bar->memcpy_bar(base + RORC_REG_SGENTRY_ADDR_LOW	, &sg_entry, sizeof(sg_entry));
	}

	// clear following BD entry (required!)
	memset(&sg_entry, 0, sizeof(sg_entry));
	bar->memcpy_bar(base + RORC_REG_SGENTRY_ADDR_LOW	, &sg_entry, sizeof(sg_entry));

close_fd:
	close(fd);
	return ret;
}

void rorcfs_dma_channel::setEnableRB( int enable )
{
	unsigned int bdcfg = getPKT( RORC_REG_DMA_CTRL );
	if ( enable )
		setPKT(RORC_REG_DMA_CTRL, ( bdcfg | (1<<3) ) );
	else
		setPKT(RORC_REG_DMA_CTRL, ( bdcfg & ~(1<<3) ) );
}

unsigned int rorcfs_dma_channel::getEnableRB()
{
	return (getPKT( RORC_REG_DMA_CTRL ) >> 3 ) & 0x01;
}


void rorcfs_dma_channel::setDMAConfig( unsigned int config)
{
	setPKT(RORC_REG_DMA_CTRL, config);	
}


unsigned int rorcfs_dma_channel::getDMAConfig()
{
	return getPKT(RORC_REG_DMA_CTRL);
}


void rorcfs_dma_channel::setMaxPayload( int size )
{
	_setMaxPayload( size );
}


void rorcfs_dma_channel::setMaxPayload()
{
	_setMaxPayload( MAX_PAYLOAD );
}


void rorcfs_dma_channel::_setMaxPayload( int size )
{
	// MAX_PAYLOAD is located in the higher WORD of 
	// RORC_REG_DMA_CTRL: [25:16], 10 bits wide
	
	assert( bar!=NULL );

	// assure valid values for "size":
	// size <= systems MAX_PAYLOAD
	assert( size<=MAX_PAYLOAD );

	unsigned int status = getPKT(RORC_REG_DMA_CTRL);

	// MAX_PAYLOAD has to be provided as #DWs
	// -> divide size by 4
	unsigned int mp_size = size>>2;
	
	// clear current MAX_PAYLOAD setting
	status &= 0x8000ffff; //clear bits 30:16
	// set new MAX_PAYLOAD size
	status |= (mp_size<<16);

	// write DMA_CTRL
	setPKT(RORC_REG_DMA_CTRL, status);
	cMaxPayload = size;
}


unsigned int rorcfs_dma_channel::getMaxPayload()
{
	assert( bar!=NULL );
	unsigned int status = getPKT(RORC_REG_DMA_CTRL);

	status = (status >> 16);
	status = status<<2;

	return status;
}


void rorcfs_dma_channel::setOffsets(
		unsigned long eboffset, unsigned long rboffset)
{
	assert (bar!=NULL);
	struct rorcfs_buffer_software_pointers offsets;
	offsets.ebdm_software_read_pointer_low = 
		(uint32_t)(eboffset & 0xffffffff);
	offsets.ebdm_software_read_pointer_high = 
		(uint32_t)(eboffset>>32 & 0xffffffff);

	offsets.rbdm_software_read_pointer_low = 
		(uint32_t)(rboffset & 0xffffffff);
	offsets.rbdm_software_read_pointer_high = 
		(uint32_t)(rboffset>>32 & 0xffffffff);

	offsets.dma_ctrl = (1<<31) | // sync pointers
		((cMaxPayload>>2)<<16) | //max_payload
		(1<<2) | // enable EB
		(1<<3) | // enable RB
		(1<<0); // enable DMA engine

	bar->memcpy_bar(base + RORC_REG_EBDM_SW_READ_POINTER_L,	
			&offsets, sizeof(offsets));
}
	


void rorcfs_dma_channel::setEBOffset( unsigned long offset )
{
	assert( bar!=NULL );
	unsigned int status;

	bar->memcpy_bar(base + RORC_REG_EBDM_SW_READ_POINTER_L,	
			&offset, sizeof(offset));
	status = getPKT(RORC_REG_DMA_CTRL);
	setPKT(RORC_REG_DMA_CTRL, status | (1<<31) );
}


unsigned long rorcfs_dma_channel::getEBOffset()
{
	unsigned long offset = 
		((unsigned long)getPKT(RORC_REG_EBDM_SW_READ_POINTER_H)<<32);
	offset += (unsigned long)getPKT(RORC_REG_EBDM_SW_READ_POINTER_L);
	return offset;
}


unsigned long rorcfs_dma_channel::getEBDMAOffset()
{
	unsigned long offset = 
		((unsigned long)getPKT(RORC_REG_EBDM_FPGA_WRITE_POINTER_H)<<32);
	offset += (unsigned long)getPKT(RORC_REG_EBDM_FPGA_WRITE_POINTER_L);
	return offset;
}


void rorcfs_dma_channel::setRBOffset( unsigned long offset )
{
	assert( bar!=NULL );
	unsigned int status;

	bar->memcpy_bar(base + RORC_REG_RBDM_SW_READ_POINTER_L,	
			&offset, sizeof(offset));
	status = getPKT(RORC_REG_DMA_CTRL);

	status = getPKT(RORC_REG_DMA_CTRL);
	setPKT(RORC_REG_DMA_CTRL, status | (1<<31) );
}


unsigned long rorcfs_dma_channel::getRBOffset()
{
	unsigned long offset = 
		((unsigned long)getPKT(RORC_REG_RBDM_SW_READ_POINTER_H)<<32);
	offset += (unsigned long)getPKT(RORC_REG_RBDM_SW_READ_POINTER_L);
	return offset;
}


unsigned long rorcfs_dma_channel::getRBDMAOffset()
{
	unsigned long offset = 
		((unsigned long)getPKT(RORC_REG_RBDM_FPGA_WRITE_POINTER_H)<<32);
	offset += (unsigned long)getPKT(RORC_REG_RBDM_FPGA_WRITE_POINTER_L);
	return offset;
}


unsigned int rorcfs_dma_channel::getEBDMnSGEntries()
{
	return (getPKT(RORC_REG_EBDM_N_SG_CONFIG) & 0x0000ffff);
}

unsigned int rorcfs_dma_channel::getRBDMnSGEntries()
{
	return (getPKT(RORC_REG_RBDM_N_SG_CONFIG) & 0x0000ffff);
}

unsigned int rorcfs_dma_channel::getDMABusy()
{
	return ((getPKT(RORC_REG_DMA_CTRL)>>7) & 0x01);
}

unsigned long rorcfs_dma_channel::getEBSize()
{
	unsigned long size = 
		((unsigned long)getPKT(RORC_REG_EBDM_BUFFER_SIZE_H)<<32);
	size += (unsigned long)getPKT(RORC_REG_EBDM_BUFFER_SIZE_L);
	return size;
}

unsigned long rorcfs_dma_channel::getRBSize()
{
	unsigned long size = 
		((unsigned long)getPKT(RORC_REG_RBDM_BUFFER_SIZE_H)<<32);
	size += (unsigned long)getPKT(RORC_REG_RBDM_BUFFER_SIZE_L);
	return size;
}

// Packetizer
void rorcfs_dma_channel::setPKT(unsigned int addr, unsigned int data)
{
	bar->set(base + addr,
			data);
}
unsigned int rorcfs_dma_channel::getPKT(unsigned int addr)
{
	return bar->get(base + addr);
}

// GTX Domain
void rorcfs_dma_channel::setGTX(unsigned int addr, unsigned int data)
{
	bar->set(base + (1<<RORC_DMA_CMP_SEL) + addr, data);
}
unsigned int rorcfs_dma_channel::getGTX(unsigned int addr)
{
	return bar->get(base + (1<<RORC_DMA_CMP_SEL) + addr);
}
