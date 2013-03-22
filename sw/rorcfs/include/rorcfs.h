#ifndef _RORCFS_RORCFS_H
#define _RORCFS_RORCFS_H

struct rorcfs_sync_range {
	void *start;
	void *end;
};

struct t_rorcfs_buffer {
	unsigned long id;
	unsigned long bytes;
	short overmap;
	short dma_direction;
};


struct rorcfs_event_descriptor {
	unsigned long offset;
	unsigned long length;
	unsigned int calc_event_size;
	unsigned int reported_event_size;
	unsigned long dummy; //do not use!
};

/**
 * struct rorcfs_dma_desc: dma address and length
 * to be transfered to the RORC
 **/
typedef struct rorcfs_dma_desc {
	unsigned long long		addr;
	unsigned long					len;
} trorcfs_dma_desc, *prorcfs_dma_desc;

#define RORCFS_DMA_FROM_DEVICE 2
#define RORCFS_DMA_TO_DEVICE 1
#define RORCFS_DMA_BIDIRECTIONAL 0


// enable PAGE_MASK and PAGE_SIZE for userspace apps
#ifndef PAGE_MASK
#define PAGE_MASK ~(sysconf(_SC_PAGESIZE) - 1)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

#define MODELSIM_SERVER "localhost"

#endif
