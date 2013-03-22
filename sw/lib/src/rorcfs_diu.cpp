/**
 * @file rorcfs_diu.cpp
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2011-09-27
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
#include "rorcfs_dma_channel.hh"
#include "rorc_registers.h"
#include "rorcfs_diu.hh"


rorcfs_diu::rorcfs_diu()
{
	bar = NULL;
	base = 0;
}


rorcfs_diu::~rorcfs_diu()
{
}


int rorcfs_diu::init(rorcfs_dma_channel *ch)
{
	bar = ch->getBar();
	base = ch->getBase();
	if (bar==NULL)
		return -1;
	else
		return 0;
}

/*
void rorcfs_diu::setDiuEnable( int enable)
{
	assert(bar!=NULL);
	int status = bar->get(base + RORC_REG_DIU_CTRL);
	if (enable)
		status |= (1<<RORC_BIT_DIU_CTRL_DIU_ENABLE);
	else
		status &= ~(1<<RORC_BIT_DIU_CTRL_DIU_ENABLE);
	bar->set(base + RORC_REG_DIU_CTRL, status);
}


unsigned int rorcfs_diu::getDiuEnable()
{
	assert(bar!=NULL);
	unsigned int ctrl = bar->get(base + RORC_REG_DIU_CTRL);
	return ( (ctrl>>RORC_BIT_DIU_CTRL_DIU_ENABLE) & 0x01 );
}


// get last sent DIU Command 
unsigned int rorcfs_diu::getCmd()
{
	return bar->get(base + RORC_REG_DIU_CMD);
}


// send DIU Command
void rorcfs_diu::setCmd( unsigned int cmd )
{
	bar->set(base + RORC_REG_DIU_CMD, cmd);
}


// get last DIU Status
unsigned int rorcfs_diu::getStatus()
{
	return bar->get(base + RORC_REG_DIU_STS0);
}


// clear DIU Status Register
void rorcfs_diu::clearStatus()
{
	bar->set(base + RORC_REG_DIU_STS0, 0);
}


// get DIU Deadtime (the time link_full_N is low)
unsigned int rorcfs_diu::getIfDeadtime()
{
	return bar->get(base + RORC_REG_DIU_DEADTIME);
}


// clear DIU Deadtime Counter
void rorcfs_diu::clearIfDeadtime()
{
	bar->set(base + RORC_REG_DIU_DEADTIME, 0);
}


// get DIU Event Count
unsigned int rorcfs_diu::getIfEventCount()
{
	return bar->get(base + RORC_REG_DIU_EC);
}


// clear DIU Event Count
void rorcfs_diu::clearIfEventCount()
{
	bar->set(base + RORC_REG_DIU_EC, 0);
}


// get DIU Interface Status:
// status[31] = riLD_N
// status[30] = riLF_N
// status[29] = link_full_N
// status[28:1] = 0
// status[1] = DIU_enable
// status[0] = busy_flag
unsigned int rorcfs_diu::getIfStatus()
{
	return bar->get(base + RORC_REG_DIU_CONFIG);
}


// set DIU Interface Busy
// valid values are 0 or 1
// when enabled (set 1) the DIU generates a roBSY_N signal to
// stop the link from receiving when link is full
// when disabled (set 0, default) the DIU will never issue a
// link_full_N, thus a roBSY_N
void rorcfs_diu::setIfBusy( unsigned int busy )
{
	bar->set(base + RORC_REG_DIU_CONFIG, busy);
}

void rorcfs_diu::setIfConfig( unsigned int config )
{
	bar->set(base + RORC_REG_DIU_CONFIG, config);
}

// get last calculated event length
unsigned int rorcfs_diu::getIfEventLengthCalc()
{
	return bar->get(base + RORC_REG_DIU_CLR_EL);
}

// clear calculated event length counter
// this has priority over incrementing the data count
// on valid data words - Don't use unless you know what 
// you're doing!
void rorcfs_diu::clearIfEventLengthCounter()
{
	bar->set(base + RORC_REG_DIU_CLR_EL, 0);
}

// get GTX sync status
unsigned int rorcfs_diu::getGtxStatusFlags()
{
	unsigned int sstatus = bar->get(base + RORC_REG_GTX_SCONFIG);
	unsigned int astatus = bar->get(base + RORC_REG_GTX_ACONFIG);
	int result = 0;

	if (sstatus != 0xffffffff) {
		if (sstatus & (1<<RORC_BIT_GTX_S_TLK_ENABLE))
			result |= GTX_PHY_ENABLED;
		if (sstatus & (1<<RORC_BIT_GTX_S_TLK_READY))
			result |= GTX_PHY_READY;
		if (sstatus & (1<<RORC_BIT_GTX_S_TLK_LOOPEN))
			result |= GTX_PHY_LOOPEN;
		if (sstatus & (1<<RORC_BIT_GTX_S_RXBYTEISALIGNED))
			result |= GTX_RXBYTEISALIGNED;
		if (sstatus & (1<<RORC_BIT_GTX_S_RXLOSSOFSYNC))
			result |= GTX_RXLOSSOFSYNC;
	} else
		result |= GTX_CLK_STOPPED;

	if (astatus & (1<<RORC_BIT_GTX_A_RXPLLLKDET))
		result |= GTX_RXPLLLKDET;
	if (astatus & (1<<RORC_BIT_GTX_A_TXPLLLKDET))
		result |= GTX_TXPLLLKDET;
	if (astatus & (1<<RORC_BIT_GTX_A_TXRESETDONE))
		result |= GTX_TXRESETDONE;
	if (astatus & (1<<RORC_BIT_GTX_A_RXRESETDONE))
		result |= GTX_RXRESETDONE;
	if (astatus & (1<<RORC_BIT_GTX_A_TXRESET))
		result |= GTX_TXRESET;
	if (astatus & (1<<RORC_BIT_GTX_A_RXRESET))
		result |= GTX_RXRESET;
	if (astatus & (1<<RORC_BIT_GTX_A_GTXRESET))
		result |= GTX_GTXRESET;
	if (astatus & (1<<RORC_BIT_GTX_A_RXELECIDLE))
		result |= GTX_RXELECIDLE;
	return result;
}


unsigned int rorcfs_diu::getGtxLoopback()
{
	return ((bar->get(base + RORC_REG_GTX_ACONFIG) >> 
				RORC_BIT_GTX_A_LOOPBACK ) &
		( (1<<RORC_BIT_GTX_A_LOOPBACK_WIDTH)-1 ) );
}

unsigned int rorcfs_diu::getGtxTxDiffCtrl()
{
	return ((bar->get(base + RORC_REG_GTX_ACONFIG) >> 
				RORC_BIT_GTX_A_TXDIFFCTRL) &
		( (1<<RORC_BIT_GTX_A_TXDIFFCTRL_WIDTH)-1 ) );
}

unsigned int rorcfs_diu::getGtxTxPreEmph()
{
	return ((bar->get(base + RORC_REG_GTX_ACONFIG) >> 
				RORC_BIT_GTX_A_TXPREEMPH) &
		( (1<<RORC_BIT_GTX_A_TXPREEMPH_WIDTH)-1 ) );
}

unsigned int rorcfs_diu::getGtxPostEmph()
{
	return ((bar->get(base + RORC_REG_GTX_ACONFIG) >> 
				RORC_BIT_GTX_A_TXPOSTEMPH) &
		( (1<<RORC_BIT_GTX_A_TXPOSTEMPH_WIDTH)-1 ) );
}

int rorcfs_diu::getGtxRxClkCorCnt()
{
	int status = bar->get(base + RORC_REG_GTX_SCONFIG);
	if (status==-1)
		return -1;
	else
		return ( (status >> RORC_BIT_GTX_S_RXCLKCORCNT) &
		( (1<<RORC_BIT_GTX_S_RXCLKCORCNT_WIDTH)-1 ) );
}

int rorcfs_diu::getGtxTxBufStatus()
{
	int status = bar->get(base + RORC_REG_GTX_SCONFIG);
	if (status==-1)
		return -1;
	else
		return ( (status >> RORC_BIT_GTX_S_TXBUFSTATUS) &
		( (1<<RORC_BIT_GTX_S_TXBUFSTATUS_WIDTH)-1 ) );
}

int rorcfs_diu::getGtxRxBufStatus()
{
	int status = bar->get(base + RORC_REG_GTX_SCONFIG);
	if (status==-1)
		return -1;
	else
		return ( (status >> RORC_BIT_GTX_S_RXBUFSTATUS) &
			( (1<<RORC_BIT_GTX_S_RXBUFSTATUS_WIDTH)-1 ) );
}

void rorcfs_diu::setGtxLoopback(unsigned int value)
{
	unsigned int status = bar->get(base + RORC_REG_GTX_ACONFIG);
	status &= ~( ((1>>RORC_BIT_GTX_A_LOOPBACK_WIDTH)-1) << 
			RORC_BIT_GTX_A_LOOPBACK );
	status += ( (value & ((1>>RORC_BIT_GTX_A_LOOPBACK_WIDTH)-1)) << 
			RORC_BIT_GTX_A_LOOPBACK );
	bar->set(base + RORC_REG_GTX_ACONFIG, status);
}

void rorcfs_diu::setGtxTxDiffCtrl(unsigned int value)
{
	unsigned int status = bar->get(base + RORC_REG_GTX_ACONFIG);
	status &= ~( ((1>>RORC_BIT_GTX_A_TXDIFFCTRL_WIDTH)-1) << 
			RORC_BIT_GTX_A_TXDIFFCTRL );
	status += ( (value & ((1>>RORC_BIT_GTX_A_TXDIFFCTRL_WIDTH)-1)) << 
			RORC_BIT_GTX_A_TXDIFFCTRL );
	bar->set(base + RORC_REG_GTX_ACONFIG, status);
}

void rorcfs_diu::setGtxTxPreEmph(unsigned int value)
{
	unsigned int status = bar->get(base + RORC_REG_GTX_ACONFIG);
	status &= ~( ((1>>RORC_BIT_GTX_A_TXPREEMPH_WIDTH)-1) << 
			RORC_BIT_GTX_A_TXPREEMPH );
	status += ( (value & ((1>>RORC_BIT_GTX_A_TXPREEMPH_WIDTH)-1)) << 
			RORC_BIT_GTX_A_TXPREEMPH );
	bar->set(base + RORC_REG_GTX_ACONFIG, status);
}

void rorcfs_diu::setGtxTxPostEmph(unsigned int value)
{
	unsigned int status = bar->get(base + RORC_REG_GTX_ACONFIG);
	status &= ~( ((1>>RORC_BIT_GTX_A_TXPOSTEMPH_WIDTH)-1) << 
			RORC_BIT_GTX_A_TXPOSTEMPH );
	status += ( (value & ((1>>RORC_BIT_GTX_A_TXPOSTEMPH_WIDTH)-1)) << 
			RORC_BIT_GTX_A_TXPOSTEMPH );
	bar->set(base + RORC_REG_GTX_ACONFIG, status);
}

unsigned int rorcfs_diu::getGtxClkCfg()
{
	return bar->get(base + RORC_REG_GTX_CLKCFG);
}
*/
