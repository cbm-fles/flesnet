/**
 * @file buffer.h
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2012-10-12
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

#ifndef _RORCFS_BUFFER_H
#define _RORCFS_BUFFER_H

void rorcfs_free_pages(
		struct rorcfs_buf *buf, 
		struct rorcfs_priv *priv);

int rorcfs_alloc_pages(
		struct rorcfs_buf *buf, 
		struct rorcfs_priv *priv);

unsigned int rorcfs_sg_coalesce(
		struct rorcfs_dma_desc **desc_buffer, 
		struct scatterlist *sglist, 
		unsigned long sglen);

#endif
