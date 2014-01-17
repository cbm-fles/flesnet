/**
 * @file buffer.c
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

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <asm-generic/dma-mapping-common.h>
#include <linux/scatterlist.h>

#include "../include/rorcfs.h"
#include "base.h"
#include "buffer.h"
#include "sysfs.h"


/**
 * sg_dump: dump scatterlist to syslog
 * @param scatterlist: list to be dumped
 * @param sglen: length of list to be dumped
 * */
void sg_dump(struct scatterlist *sglist, unsigned long sglen)
{
	struct scatterlist *sgptr;
	unsigned long i;

	for_each_sg(sglist, sgptr, sglen, i)
	{
		rorcfs_debug( "%s ***sgdump %ld: %lx, %x, %llx, %x\n", DRV_NAME, i,
				sgptr->page_link, sgptr->length, 
				sg_dma_address(sgptr), sg_dma_len(sgptr) );
	}
}


/**
 * rorcfs_sg_sort: sort scatterlist by its page addresses
 * 
 * observation: sglist is often created with neighbouring, but
 * descending page adresses. Mapping those for DMA results in 
 * sgentries that cannot be combined because of their inverse 
 * ordering. If the sglist is ordered by page addresses before
 * mapping it for dma, adjacent page adresses more often result 
 * in adjacent physical addresses, allowing the sgentries to be
 * merged.
 *
 * @param sglist: struct scatterlist
 * @param sglen: length of the scatterlist
 **/
void rorcfs_sg_sort(struct scatterlist *sglist, unsigned long sglen)
{
	struct scatterlist *sgptr, sgtmp;
	unsigned long i;
	bool swapped;

	sg_dump(sglist, sglen);
	rorcfs_debug("%s sorting sglist...\n", DRV_NAME);
	
	do {
		swapped = false;

		// iterate over scatter gather list entries
		for_each_sg(sglist, sgptr, sglen, i) {
			
			// make sure the next entry exists
			if (i < sglen-1) {
				// compare page addresses
				if ( sglist[i].page_link > sglist[i+1].page_link )
				{
					//swap entries
					sgtmp = sglist[i];
					sglist[i] = sglist[i+1];
					sglist[i+1] = sgtmp;
					swapped = true;
				}
			}
		}

	} while (swapped==true);
	
	sg_dump(sglist, sglen);
}


/**
 * rorcfs_sg_coalesce: reduce the number of sg-entries by combining 
 * adjacent entries
 */
unsigned int rorcfs_sg_coalesce(struct rorcfs_dma_desc **desc_buffer, struct scatterlist *sglist, unsigned long sglen)
{
	struct scatterlist *sgptr;
	unsigned long i, idx;
	unsigned long nents = sglen;
	struct rorcfs_dma_desc *mem;
	unsigned long size;

	// get number of combineable sg entries
	for_each_sg(sglist, sgptr, sglen, i) {
		if( i < (sglen-1) && 
				(sg_dma_address(&sglist[i]) + sg_dma_len(&sglist[i])) == 
				sg_dma_address(&sglist[i+1]) ) {
			// sg-entries can be combined
			nents--;
		}
	}

	size = (nents * sizeof(struct rorcfs_dma_desc));
	if ( size & (PAGE_SIZE-1) ) //not multiple of page size
		size = ((size>>PAGE_SHIFT) + 1)<<PAGE_SHIFT;
	
	// allocate memory for the dma_list struct
	mem = vmalloc_user(size);
	if (mem == NULL) {
		printk(KERN_ERR "%s sg_coalesce failed to allocate mem\n", DRV_NAME);
		return -ENOMEM;
	}

	// start with first dma_addr_t
	idx = 0;
	mem[idx].addr = sg_dma_address(&sglist[0]);
	mem[idx].len = 0;

	// iterate over all sg entries
	for_each_sg(sglist, sgptr, sglen, i) {
		if ( ( i < (sglen-1) && 
				(sg_dma_address(&sglist[i]) + sg_dma_len(&sglist[i]) == 
						sg_dma_address(&sglist[i+1])) ) || 
				i == (sglen-1) ) {
			// entries can be combined, keep start address, increment length
			mem[idx].len += sg_dma_len(&sglist[i]);
		} else {
			// cannot be combined, fill length of previous entry
			mem[idx].len += sg_dma_len(&sglist[i]);
			// increment index and initialize next entries start address
			idx++;
			
			// if not yet at the end of the list -> initialize next segement
			if( idx < nents ) { 
				mem[idx].addr =  sg_dma_address(&sglist[i+1]);
				mem[idx].len = 0;
			}
		}
	}

	// print dma_list entries to stdout
	for(i=0;i<nents;i++)
		rorcfs_debug("%s rorcfs_sg_coalesce: addr %llx len %lx\n",
				DRV_NAME, mem[i].addr, mem[i].len);

	rorcfs_debug("%s rorcfs_sg_coalesce: nents=%lu\n", 
			DRV_NAME, nents);
	*desc_buffer = mem;
	return nents;
}



/**
 * rorcfs_free_pages: release allocated pages
 * @buf: rorcfs_buf struct
 * @priv: rorcfs_priv struct for dma unmapping
 **/
void rorcfs_free_pages(struct rorcfs_buf *buf, struct rorcfs_priv *priv)
{
	int i;
	size_t alloc_size;
	struct page *page;

	// clear event buffer
	if(buf->sglen) {

		dma_unmap_sg(&priv->pdev->dev, buf->sglist, 
				buf->sglen, buf->dma_direction);

		for (i=0; i<buf->sglen; i++) {
			alloc_size = sg_dma_len(&buf->sglist[i]);
			page = sg_page(&buf->sglist[i]);

			do {
				ClearPageReserved(page++);
			} while (alloc_size -= PAGE_SIZE);

			__free_pages(sg_page(&buf->sglist[i]), 
					get_order(sg_dma_len(&buf->sglist[i])));
		}
		buf->sglen = 0;
	}

	if(buf->nents)
		vfree(buf->fpga_sglist);
	buf->fpga_sglist = NULL;
	buf->nents = 0;

	if(buf->sglist)
		vfree(buf->sglist);
	buf->sglist = NULL;
	
	buf->nr_pages = 0;
}


/**
 * rorcfs_alloc_pages: allocate pages for DMA
 * @buf: rorcfs_buf struct
 * @priv: rorcfs_priv struct for dma unmapping
 *
 * return 0 on success
 **/
int rorcfs_alloc_pages(struct rorcfs_buf *buf, 
		struct rorcfs_priv *priv) 
{
	int err;
	int order;
	int npages;
	struct page *page;
	size_t alloc_size;

	npages = buf->nr_pages;

	// allocate buffer for scatter/gather list
	buf->sglist = vmalloc(npages * sizeof(*buf->sglist));
	if (!buf->sglist) {
		printk(KERN_ERR "%s buffer %ld, alloc_pages: vmalloc(sglist) failed\n", 
				DRV_NAME, buf->bufid);
		err = -ENOMEM;
		goto fail;
	}
	// initialize scatterlist
	buf->sglen = 0;
	sg_init_table(buf->sglist, npages);

	// allocation ist done with multiples of 2^order pages
	// get starting order for allocations
	order = get_order(npages<<PAGE_SHIFT);

	// limit maximum order to 10
	if (order>10)
		order = 10;
	
	while(npages > 0) {
		
		// reduce order if nr. of remaining pages 
		// is smaller than 2^order pages
		while( 1 << order > npages)
			order--;
	
		// allocate (2^order) pages
		// TODO: select memory type on allocation
		page = alloc_pages(__GFP_HIGHMEM, order);
		//page = alloc_pages(__GFP_DMA32, order);
		if (!page) {
			// allocation failed -> reduce order and try again
			rorcfs_debug("%s buffer %ld, alloc_pages /w order=%d failed\n",	
					DRV_NAME, buf->bufid, order);
			order--;
			if(order<0) {
				err = -ENOMEM;
				goto fail;
			}
		}	else {
			// allocation succeeded, add sglist entry
			sg_set_page(&buf->sglist[buf->sglen], page, PAGE_SIZE << order, 0);
			// adjust nr. of remaining pages
			npages -= 1 << order;
			// increment sglist index
			buf->sglen++;

			alloc_size = (PAGE_SIZE << order);

			// clear allocated pages
			memset( page_address(page), 0, alloc_size );

			// mark allocated pages reserved
			do {
				SetPageReserved(page++);
			} while (alloc_size -= PAGE_SIZE);

			rorcfs_debug("%s buffer %ld, alloc_pages /w order=%d succeeded, "
					"id=%lu, %d pages remaining\n",
					DRV_NAME, buf->bufid, order, buf->sglen-1, npages);
		}
	}

	// sort sglist by page addresses
	rorcfs_sg_sort(buf->sglist, buf->sglen);

	rorcfs_debug( "%s buffer %ld, mapping sglist for DMA...\n", 
			DRV_NAME, buf->bufid );

	// map allocated pages for DMA
	err = dma_map_sg(&priv->pdev->dev, buf->sglist, 
			buf->sglen, buf->dma_direction);

	if (!err) {
		printk(KERN_ERR "%s buffer %ld, dma_map_sg() failed\n", 
				DRV_NAME, buf->bufid);
		err = -EINVAL;
		goto fail;
	}


	// coalesce sg-list, combine adjacent dma_addresses
	buf->nents = rorcfs_sg_coalesce(&buf->fpga_sglist, buf->sglist, buf->sglen);
	if(buf->nents <= 0 || buf->fpga_sglist == NULL) {
		printk(KERN_ERR "%s buffer %ld, failed to coalesce sglist, "
				"%lu entries\n", DRV_NAME, buf->bufid, buf->nents);
		err = -ENOMEM;
		goto fail;
	}
	
	rorcfs_debug( "%s buffer %ld, nr_pages=%ld, sglen=%ld, overmap=%d, "
			"nents=%ld, dma_direction=%d\n", DRV_NAME, buf->bufid, 
			buf->nr_pages, buf->sglen, buf->overmap, buf->nents, 
			buf->dma_direction );
	sg_dump(buf->sglist, buf->sglen);

	return 0;

fail:
	rorcfs_free_pages(buf, priv);
	return err;
}

