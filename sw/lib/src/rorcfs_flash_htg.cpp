/**
 * @file rorcfs_flash_htg.cpp
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2012-02-29
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
 * */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "rorcfs_bar.hh"
#include "rorcfs_flash_htg.hh"

rorcfs_flash_htg::rorcfs_flash_htg(rorcfs_bar *flashbar)
{
	bar = NULL;
	bar = flashbar;
	read_state = 0;

	assert(bar!=NULL);
}

rorcfs_flash_htg::~rorcfs_flash_htg()
{
	bar = NULL;
	read_state = 0;
}

void rorcfs_flash_htg::setReadState(
		unsigned short state, unsigned int addr)
{
	bar->set16(addr, state);
	read_state = state;
}

unsigned short rorcfs_flash_htg::getStatusRegister(
		unsigned int blkaddr)
{
	if (read_state!=FLASH_READ_STATUS)
		setReadState(FLASH_READ_STATUS, blkaddr);

	return bar->get16(blkaddr);
}

void rorcfs_flash_htg::clearStatusRegister(
		unsigned int blkaddr)
{
	bar->set16(blkaddr, FLASH_CMD_CLR_STS);
	setReadState(FLASH_READ_ARRAY, blkaddr);
	//read_state = 0; //TODO: what is correct here?
}

unsigned short rorcfs_flash_htg::getManufacturerCode()
{
	if(read_state!=FLASH_READ_IDENTIFIER)
		setReadState(FLASH_READ_IDENTIFIER, 0x00);
	return bar->get16(0x00);
}

unsigned short rorcfs_flash_htg::getDeviceID()
{
	if(read_state!=FLASH_READ_IDENTIFIER)
		setReadState(FLASH_READ_IDENTIFIER, 0x00);
	return bar->get16(0x01);
}

unsigned short rorcfs_flash_htg::getBlockLockConfiguration(
		unsigned int blkaddr)
{
	if(read_state!=FLASH_READ_IDENTIFIER)
		setReadState(FLASH_READ_IDENTIFIER, blkaddr);
	return bar->get16(blkaddr + 0x02);
}

unsigned short rorcfs_flash_htg::getReadConfigurationRegister()
{
	if(read_state!=FLASH_READ_IDENTIFIER)
		setReadState(FLASH_READ_IDENTIFIER, 0x00);
	return bar->get16(0x05);
}

unsigned short rorcfs_flash_htg::get(unsigned int addr)
{
	setReadState(FLASH_READ_ARRAY, addr);
	return bar->get16(addr);
}

int rorcfs_flash_htg::programWord(
		unsigned int addr, unsigned short data)
{
	unsigned short status = 0;
	unsigned int timeout = CFG_FLASH_TIMEOUT;

	// word program setup
	bar->set16(addr, FLASH_CMD_PROG_SETUP);

	// addr, data
	bar->set16(addr, data);
	read_state = FLASH_READ_STATUS;
	status = bar->get16(addr);

	while( (status & FLASH_PEC_BUSY)==0 ) {
		usleep(100);
		status = bar->get16(addr);
		timeout--;
		if ( timeout==0 )
			return -1;
	}

	if(status==FLASH_PEC_BUSY) {
		clearStatusRegister(addr);
		return 0;
	} else
		return -1;
}

int rorcfs_flash_htg::programBuffer(
		unsigned int addr,
		unsigned short length,
		unsigned short *data)
{
	int i;
	unsigned short status;
	unsigned int timeout = CFG_FLASH_TIMEOUT;
	unsigned int blkaddr = addr & CFG_FLASH_BLKMASK;


	// check if block is locked
	if ( getBlockLockConfiguration(blkaddr) )
		unlockBlock(blkaddr);

	//write to buffer command
	bar->set16(blkaddr, FLASH_CMD_BUFFER_PROG); 
	read_state = FLASH_READ_STATUS;

	// read status register
	status = bar->get16(blkaddr);
	if ( (status & FLASH_PEC_BUSY) == 0 ) // check if WSM is ready
		return -1;

	// write word count
	bar->set16(blkaddr, length-1);

	//write {buffer-data, start-addr}
	bar->set16(addr, data[0]);

	for ( i=1; i<length; i++ ) {
		bar->set16(addr+i, data[i]);
		//printf("writing %04x to addr %x\n", data[i], addr+i);
	}

	// write {confirm, blkaddr}
	bar->set16(blkaddr, FLASH_CMD_CONFIRM);

	// read status register
	status = bar->get16(blkaddr);
	//printf("programBuffer STS: %04x\n", status);

	while ( !(status & FLASH_PEC_BUSY) ) { // device is busy
		usleep(100);
		status = bar->get16(blkaddr);
		timeout--;
		if ( timeout==0 ) {
			printf("programBuffer timed out (%04x)\n", status);
			return -1;
		}
	}
	if ( status != FLASH_PEC_BUSY ) { // SR.5 or SR.4 nonzero -> program/erase/sequence error
		printf("programBuffer failed: ");
		switch (status & 0x0030) {
			case 0x0030:
				printf("Command Sequence Error - command aborted\n");
				break;
			case 0x0010:
				printf("Program Error - operation aborted\n");
				if ( status & 0x0002 ) 
					printf("Block locked during program\n");
				break;
			case 0x0020:
				printf("Erase Error - operation aborted\n");
				break;
			default: 
				printf("Unknown Error - Software Bug!\n");
				break;
		}
		return -1;	
	}
	//return to ReadArry mode
	setReadState(FLASH_READ_ARRAY, blkaddr);
	return 0;
}

int rorcfs_flash_htg::eraseBlock(unsigned int blkaddr)
{
	unsigned short status;
	unsigned int timeout = CFG_FLASH_TIMEOUT;

	// erase command
	bar->set16(blkaddr, FLASH_CMD_BLOCK_ERASE_SETUP);

	//confim command
	bar->set16(blkaddr, FLASH_CMD_CONFIRM);
	read_state = FLASH_READ_STATUS;

	status = bar->get16(blkaddr);
	//printf("erase STS: %04x\n", status);

	while ( (status & FLASH_PEC_BUSY)==0 ) {
		usleep(100);
		status = bar->get16(blkaddr);
		timeout--;
		if ( timeout==0 ) {
			printf("eraseBlock timeout\n");
			return -1;
		}
	}
	//printf("erase time: %d x 100us\n", CFG_FLASH_TIMEOUT - timeout);
	if ( status == FLASH_PEC_BUSY ) {
		clearStatusRegister(blkaddr);
		return 0;
	}	else
		return -1;
}

void rorcfs_flash_htg::programSuspend(unsigned int blkaddr)
{
	bar->set16(blkaddr, FLASH_CMD_PROG_ERASE_SUSPEND);
}

void rorcfs_flash_htg::programResume(unsigned int blkaddr)
{
	bar->set16(blkaddr, FLASH_CMD_CONFIRM);
}

void rorcfs_flash_htg::lockBlock(unsigned int blkaddr)
{
	bar->set16(blkaddr, FLASH_CMD_BLOCK_LOCK_SETUP); // block lock setup
	bar->set16(blkaddr, FLASH_CMD_BLOCK_LOCK_CONFIRM); // block lock
}

void rorcfs_flash_htg::unlockBlock(unsigned int blkaddr)
{
	bar->set16(blkaddr, FLASH_CMD_BLOCK_LOCK_SETUP); // block lock setup
	bar->set16(blkaddr, FLASH_CMD_CONFIRM); // block unlock
}

void rorcfs_flash_htg::setConfigReg(unsigned short value)
{
	bar->set16(value, FLASH_CMD_BLOCK_LOCK_SETUP);
	bar->set16(value, FLASH_CMD_CFG_REG_CONFIRM);
}

int rorcfs_flash_htg::blankCheck(unsigned int blkaddr)
{
	unsigned short status;
	unsigned int timeout = CFG_FLASH_TIMEOUT;

	bar->set16(blkaddr, FLASH_CMD_BC_SETUP);
	bar->set16(blkaddr, FLASH_CMD_BC_CONFIRM);
	read_state = FLASH_READ_STATUS;
	status = bar->get16(blkaddr);

	// wait for blankCheck to complete
	while ( (status & FLASH_PEC_BUSY)==0 ) {
		usleep(100);
		status = bar->get16(blkaddr);
		timeout--;
		if ( timeout==0 ) {
			printf("blankCheck timeout: %08x\n", status);
			return -1;
		}
	}
	clearStatusRegister(blkaddr);
	printf("Blank Check addr %08xx=%04x\n",
			blkaddr, status);
	// SR5==1: block not empty -> return 0
	// SR5==0: block empty -> return 1
	return ~(status>>5) & 0x01;
}

unsigned int rorcfs_flash_htg::getBlockAddress( unsigned int addr )
{
	/** memory organization:
	 * block 130: 0x000000 - 0x00ffff
	 * block 129: 0x001000 - 0x01ffff
	 * ....
	 * block 4: 0x7e0000 - 0x7effff
	 * block 3: 0x7f0000 - 0x7f3fff
	 * block 2: 0x7f4000 - 0x7f7fff
	 * block 1: 0x7f8000 - 0x7fbfff
	 * block 0: 0x7fc000 - 0x7fffff
	 * */
	if (addr <= 0x7effff) 
		return (addr & 0xffff0000);
	else 
		return (addr & 0xffffc000);
}

unsigned int rorcfs_flash_htg::getBankAddress( unsigned int addr )
{
	/** bank 0: 0x000000 - 0x080000 **/
	return (addr & 0xff800000);
}

