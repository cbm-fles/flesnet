 /**
 * @file rorcfs_dma_monitor.hh
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

#ifndef _RORCLIB_RORCFS_DMA_MONITOR_H
#define _RORCLIB_RORCFS_DMA_MONITOR_H

/**
 * @class rorcfs_dma_monitor
 * @brief Modelsim FLI DMA Monitor Client
 **/
class rorcfs_dma_monitor
{
	public:
		rorcfs_dma_monitor();
		~rorcfs_dma_monitor();

	private:
		int sockfd;
		pthread_t dma_mon_p;
};

#endif
