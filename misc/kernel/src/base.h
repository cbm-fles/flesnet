/**
 * @file base.h
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

#ifndef _RORCFS_BASE_H
#define _RORCFS_BASE_H

#define DRV_NAME "rorcfs"

#ifdef DEBUG
#define rorcfs_debug(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define rorcfs_debug(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

/**
 * struct rorcfs_buf: buffer descriptor struct
 * created for each buffer allocated;
 * chained with a list
 **/
struct rorcfs_buf {
	struct list_head 			list;
	unsigned long					bufid;

	unsigned long					nr_pages;
	struct scatterlist		*sglist;
	unsigned long 				sglen;
	struct rorcfs_dma_desc *fpga_sglist;
	unsigned long					nents;

	/* use overmapping */
	bool									overmap;

	/* DMA direction */
	enum dma_data_direction dma_direction;

	/* sysfs subdirectory kobject*/
	struct kobject				*kobj;
	struct bin_attribute	*mem_attr;

	/* vma stuff */
	int										vma_count;
	};


/**
 * rorcfs_priv: drivers private struct
 **/
struct rorcfs_priv {
	struct pci_dev				*pdev;
	struct rorcfs_buf			*buf;

	/* sysfs directory "mem" */
	struct kobject				*kobj;
	int										vma_count;
	};

#endif
