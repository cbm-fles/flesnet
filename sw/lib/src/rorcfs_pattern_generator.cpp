/**
 * @file rorcfs_pattern_generator.cpp
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
#include "rorcfs_diu.hh"
#include "rorcfs_pattern_generator.hh"
#include "rorc_registers.h"


rorcfs_pattern_generator::rorcfs_pattern_generator()
{
	bar = NULL;
	base = 0;
}

rorcfs_pattern_generator::~rorcfs_pattern_generator()
{
}

int rorcfs_pattern_generator::init( rorcfs_diu *diu )
{
	bar = diu->getBar();
	base = diu->getBase();
	if (bar==NULL)
		return -1;
	else
		return 0;
}

void rorcfs_pattern_generator::setMode( 
		unsigned int mode )
{
	assert(bar!=NULL);
	//bar->set(base + RORC_REG_DDL_PG_MODE, mode);
}

unsigned int rorcfs_pattern_generator::getMode()
{
	assert(bar!=NULL);
	return 0;//bar->get(base + RORC_REG_DDL_PG_MODE);
}

void rorcfs_pattern_generator::setWaitTime( unsigned int wait_time )
{
	assert(bar!=NULL);
	bar->set(base + RORC_REG_DDL_PG_WAIT_TIME, wait_time);
}

unsigned int rorcfs_pattern_generator::getWaitTime()
{
	assert(bar!=NULL);
	return bar->get(base + RORC_REG_DDL_PG_WAIT_TIME);
}

void rorcfs_pattern_generator::setNumEvents( unsigned int num )
{
	assert(bar!=NULL);
	bar->set(base + RORC_REG_DDL_PG_NUM_EVENTS, num);
}

unsigned int rorcfs_pattern_generator::getNumEvents()
{
	assert(bar!=NULL);
	return bar->get(base + RORC_REG_DDL_PG_NUM_EVENTS);
}

void rorcfs_pattern_generator::setEventLength( unsigned int length )
{
	assert(bar!=NULL);
	bar->set(base + RORC_REG_DDL_PG_EVENT_LENGTH, length);
}

unsigned int rorcfs_pattern_generator::getEventLength()
{
	assert(bar!=NULL);
	return bar->get(base + RORC_REG_DDL_PG_EVENT_LENGTH);
}

void rorcfs_pattern_generator::setPattern( unsigned int pattern )
{
	assert(bar!=NULL);
	bar->set(base + RORC_REG_DDL_PG_PATTERN, pattern);
}

unsigned int rorcfs_pattern_generator::getPattern()
{
	assert(bar!=NULL);
	return bar->get(base + RORC_REG_DDL_PG_PATTERN);
}




	
