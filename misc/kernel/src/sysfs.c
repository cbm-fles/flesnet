/**
 * @file sysfs.c
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
#include <linux/sysfs.h>
#include <linux/version.h>

#include "../include/rorcfs.h"
#include "base.h"
#include "buffer.h"
#include "sysfs.h"

/**************************************************
 * sysfs attributes
 **************************************************/

/* DMA memory region: RORC writes event data into this buffer */
static RORC_BIN_ATTR(mem, 0, S_IRUGO|S_IWUGO, 
		rorcfs_attr_bin_read, NULL, rorcfs_attr_bin_mmap);

/* sync: initiate a dma_sync_for_cpu of a buffer range
 * feed with struct rorcfs_attr_sync_range */
static RORC_BIN_ATTR(sync, sizeof(struct rorcfs_sync_range), S_IWUGO, 
		NULL, rorcfs_attr_bin_write, NULL);

/* sglist: export sg-list to userspace */
static RORC_BIN_ATTR(sglist, 0, S_IRUGO, 
		rorcfs_attr_bin_read, NULL, rorcfs_mmap_virtual);

/* overmapped: export overmap-flag to userspace */
static RORC_BIN_ATTR(overmapped, 4, S_IRUGO,
		rorcfs_attr_bin_read, NULL, NULL);

/* overmapped: export DMA direction flag to userspace */
static RORC_BIN_ATTR(dma_direction, 4, S_IRUGO,
		rorcfs_attr_bin_read, NULL, NULL);


/* list of the attributes above */
static 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
const 
#endif
struct bin_attribute *rorcfs_sysfs_buf_bin_attrs[] = {
	&attr_bin_mem,
	&attr_bin_sync,
	&attr_bin_sglist,
	&attr_bin_overmapped,
	&attr_bin_dma_direction,
	NULL,
};


/**
 * sysfs attribute for each bar
 * defined in base.c
 * */
extern struct bin_attribute attr_bin_bar[PCI_NUM_RESOURCES];


/**
 * rorcfs_sysfs_buf_attr_init: create rorcfs_buf sysfs entries
 * @kobj: attribute kobject
 * @kobj_parent: directory parent
 * @bufid: id of the new buffer
 *
 * returns 0 on success
 **/
int rorcfs_sysfs_buf_attr_init(struct rorcfs_buf *buf, 
		struct kobject *kobj_parent)
{
	int err;
	char *name = vmalloc(64);
	sprintf(name, "%03lu", buf->bufid);

	// assign the buffer size as sysfs file size
	// use double the size if overmapped
	if (buf->overmap)
		attr_bin_mem.size = 2*(buf->nr_pages * PAGE_SIZE);
	else
		attr_bin_mem.size = buf->nr_pages * PAGE_SIZE;

	attr_bin_sglist.size = (buf->nents * sizeof(struct rorcfs_dma_desc));

	/* add directory with parent=mmap for each buffer */
	buf->kobj = kobject_create_and_add(name, kobj_parent);
	vfree(name);
	if(buf->kobj == NULL) {
		return -ENOMEM;
	}

	//returns 0 on success, -EEXIST if extry already exists
	err = rorcfs_sysfs_create_bin_files(buf->kobj, 
			rorcfs_sysfs_buf_bin_attrs);
	if (err) {
		kobject_put(buf->kobj);
	}

	return err;
}

/**
 * rorcfs_sysfs_buf_attr_release: release rorcfs_buf sysfs entries
 **/
void rorcfs_sysfs_buf_attr_release(struct kobject *kobj)
{
	if(kobj) {
		/* remove attributes */
		rorcfs_sysfs_remove_bin_files(kobj, rorcfs_sysfs_buf_bin_attrs);
		/* release rorcfs_buf subdirectory */
		kobject_put(kobj);
	}
}


/**
 * *to_rorcfs_priv: return pointer to corresponding struct rorcfs_priv
 * @kobj: kobject of the addressed attribut
 *
 * returns pointer to struct rorcfs_priv
 **/
struct rorcfs_priv *to_rorcfs_priv(struct kobject *kobj)
{
	/* calling kobj resides in [device]/mmap/[bufid] */
	struct device *dev = 
		container_of(kobj->parent->parent, struct device, kobj);
	return dev_get_drvdata(dev);
}



/**
 * *to_rorcfs_buf: return pointer to corresponding struct rorcfs_buf
 * @kobj: kobject of the addressed attribut
 *
 * returns pointer to struct rorcfs_buf of NULL on error
 **/
struct rorcfs_buf *to_rorcfs_buf(struct kobject *kobj)
{
	/* calling kobj resides in [device]/mmap/[bufid] */
	struct device *dev = 
		container_of(kobj->parent->parent, struct device, kobj);
	struct rorcfs_priv *priv = dev_get_drvdata(dev);
	struct rorcfs_buf *buf;

	/* find corresponding rorcfs_buf struct */
	list_for_each_entry(buf, &priv->buf->list, list) {
		if(buf->kobj == kobj) {
			return buf;
		}
	}
	// list_for_each_entry could not find the corresponding struct,
	// can only happen if called for a wrong attribute
	BUG_ON(buf->kobj != kobj);
	return NULL;
}


/**************************************************
 * sysfs bin_attribute fops
 **************************************************/

/**
 * rorcfs_attr_bin_read: read from binary attribute
 **/
	ssize_t rorcfs_attr_bin_read(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count)
{
	struct rorcfs_buf *buf = to_rorcfs_buf(kobj);
	int tmpdir = 0;

	// read from "sglist"
	if ( attr == &attr_bin_sglist ) {

		if(count!=sizeof(struct rorcfs_dma_desc) || offset%count!=0)
			return -EINVAL;

		if (buf->fpga_sglist == NULL)
			return -EPERM;

		memcpy(buffer, (buf->fpga_sglist + (offset>>4)), count);

	} else // read from "overmapped"
		if ( attr == &attr_bin_overmapped ) {

			if(count!=sizeof(int) || offset%count!=0)
				return -EINVAL;

			memcpy(buffer, &buf->overmap, count);

		}  else // read from "dma_direction"
			if ( attr == &attr_bin_dma_direction ) {

				if(count!=sizeof(int) || offset%count!=0)
					return -EINVAL;

				switch ( buf->dma_direction ) {
					case DMA_BIDIRECTIONAL:
						tmpdir = RORCFS_DMA_BIDIRECTIONAL;
						break;
					case DMA_TO_DEVICE:
						tmpdir = RORCFS_DMA_TO_DEVICE;
						break;
					case DMA_FROM_DEVICE:
						tmpdir = RORCFS_DMA_FROM_DEVICE;
						break;
					default:
						tmpdir = -1;
						break;
				}

				memcpy(buffer, &tmpdir, count);

			} ;

	/*rorcfs_debug("%s attr_bin_read %s(%ld), read %lu bytes"
			" from offset %llx\n", DRV_NAME, attr->attr.name, 
			buf->bufid, count, offset);*/

	return count;
}

/**
 * rorcfs_attr_bin_write: write to binary attribute
 * TODO: this code is untested!
 **/
	ssize_t rorcfs_attr_bin_write(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count)
{
	struct rorcfs_priv *priv = to_rorcfs_priv(kobj);
	struct rorcfs_buf *buf = to_rorcfs_buf(kobj);

	if (offset!=0 || count > sizeof(unsigned long))
		return -EINVAL;

	// a.t.m. only allowed to "sync"
	if ( attr == &attr_bin_sync ) {
		dma_sync_sg_for_cpu(&priv->pdev->dev, buf->sglist, 
				buf->sglen, buf->dma_direction);
		rorcfs_debug("%s synced sglist of buffer %ld to CPU\n",
				DRV_NAME, buf->bufid);
	}

	return count;
}





/**************************************************
 * vma page fault handling
 ***************************************************/

void rorcfs_vma_open(struct vm_area_struct *vma)
{
	struct rorcfs_buf *buf = vma->vm_private_data;
	buf->vma_count++;
}

void rorcfs_vma_close(struct vm_area_struct *vma)
{
	struct rorcfs_buf *buf = vma->vm_private_data;
	buf->vma_count--;
}

static const struct vm_operations_struct rorcfs_vm_ops = {
	.open = rorcfs_vma_open,
	.close = rorcfs_vma_close,
};


/**
 * rorcfs_mmap_virtual: remap fpga_sglist to userspace
 **/
	int rorcfs_mmap_virtual(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj, 
			struct bin_attribute *attr, struct vm_area_struct *vma)
{
	struct rorcfs_buf *buf = to_rorcfs_buf(kobj);
	void *addr = buf->fpga_sglist;
	int ret = 0;

	vma->vm_private_data = buf;
	vma->vm_ops = &rorcfs_vm_ops;

	ret = remap_vmalloc_range(vma, addr, 0);
	if (ret)
		printk(KERN_ERR "Remapping vmalloc memory, error %d\n", ret);

	return ret;
}


/**
 * rorcfs_attr_bin_mmap: memory map a userspace buffer to the
 * (kernel virtual) DMA buffer
 **/
	int rorcfs_attr_bin_mmap(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj, 
			struct bin_attribute *attr, struct vm_area_struct *vma)
{
	struct rorcfs_buf *buf = to_rorcfs_buf(kobj);
	int err=0, i, j;
	unsigned long size = 0;
	unsigned long virt_len = 0;

	/* "Mapping of physical memory [...] needs pgprot_noncached() to 
	 * ensure that IO memory is not cached. Without pgprot_noncached(), 
	 * it (accidentally) works on x86 and arm, but fails on PPC"
	 * Hans Koch (UIO), lkml */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	/*rorcfs_debug("%s rorcsf_bin_mmap vm_start:%lu, vm_end:%lu, size.%lu\n",
			DRV_NAME, vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start);*/

	// get virtual buffer size
	if ( buf->overmap )
		virt_len = 2*buf->sglen;
	else
		virt_len = buf->sglen;

	// access to mem file
	if ( attr == &attr_bin_mem ) {
		size = 0;

		// iterate over sglist twice to overmap the buffer to the file
		for ( j=0; j<virt_len; j++ ) {

			// keep track of target for overmapped buffers
			if ( buf->overmap )
				i = j % buf->sglen;
			else
				i = j;

			err = remap_pfn_range(
					vma, vma->vm_start + size,
					page_to_pfn(sg_page(&buf->sglist[i])),
					sg_dma_len(&buf->sglist[i]),
					vma->vm_page_prot );

			/*rorcfs_debug("%s remap_pfn_range (%ld) start:%lx pa:%lx, "
					"len:%x - UC:%ld, UC-:%ld, WB:%ld, WC:%ld\n",
					DRV_NAME, buf->bufid, vma->vm_start + size, 
					page_to_pfn(sg_page(&buf->sglist[i])),
					sg_dma_len(&buf->sglist[i]),
					(pgprot_val(vma->vm_page_prot) & _PAGE_CACHE_UC),
					(pgprot_val(vma->vm_page_prot) & _PAGE_CACHE_UC_MINUS),
					(pgprot_val(vma->vm_page_prot) & _PAGE_CACHE_WB),
					(pgprot_val(vma->vm_page_prot) & _PAGE_CACHE_WC)
				    );*/

			if (err)
				goto out;
			size += sg_dma_len(&buf->sglist[i]);
		}
	}

out:
	return err;
}



/* BAR access ***************************************/

void rorcfs_bar_vma_open(struct vm_area_struct *vma)
{
	struct rorcfs_priv *priv = vma->vm_private_data;
	priv->vma_count++;
}

void rorcfs_bar_vma_close(struct vm_area_struct *vma)
{
	struct rorcfs_priv *priv = vma->vm_private_data;
	priv->vma_count--;
}

	int rorcfs_bar_mmap(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj, 
			struct bin_attribute *attr, struct vm_area_struct *vma)
{
	struct device *dev = 
		container_of(kobj->parent, struct device, kobj);
	struct rorcfs_priv *priv = dev_get_drvdata(dev);
	unsigned long addr, size;
	int bar = -1;
	int i;

	for(i=0; i<PCI_NUM_RESOURCES; i++) {
		if ( attr == &attr_bin_bar[i] ) {
			bar = i;
			break;
		}
	}

	if ( bar==-1 ) {
		printk(KERN_ERR "%s rorcfs_attr_bar_write invalid BAR %s\n",
				DRV_NAME, attr->attr.name);
		return -EINVAL;
	}

	addr = pci_resource_start(priv->pdev, bar);
	size = pci_resource_len(priv->pdev, bar);

	/*rorcfs_debug("%s mmap addr: %lx, size:%lx\n", 
			DRV_NAME, addr, size);*/
	//vma->vm_flags |= VM_IO | VM_RESERVED; //this is set by remap_pfn_range()
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return remap_pfn_range(vma,
			vma->vm_start,
			addr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot);
}



/**
 * rorcfs_attr_bar_read: read from mapped BAR in mmap/bar[0-6]
 *
 * returns byte count on success, -EINVAL on error
 **/
	ssize_t rorcfs_attr_bar_read(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count)
{
	struct device *dev = 
		container_of(kobj->parent, struct device, kobj);
	struct rorcfs_priv *priv = dev_get_drvdata(dev);

	unsigned long size;
	void __iomem *base;
	int bar = -1;
	int i;

	for(i=0; i<PCI_NUM_RESOURCES; i++) {
		if ( attr == &attr_bin_bar[i] ) {
			bar = i;
			break;
		}
	}

	if ( bar==-1 ) {
		printk(KERN_ERR "%s rorcfs_attr_bar_write invalid BAR %s\n",
				DRV_NAME, attr->attr.name);
		return -EINVAL;
	}

	/*rorcfs_debug("%s attr_bar_read from BAR%d, offset=%llu, size=%lu\n",
			DRV_NAME, bar, offset, count);*/

	size =  pci_resource_len(priv->pdev, bar);

	// check if request is within the BAR
	if (offset + count >  size) {
		printk(KERN_ERR "%s rorcfs_attr_bar_read offset + count > size\n",
				DRV_NAME);
		return -EINVAL;
	}

	// get base address
	base = pci_ioremap_bar(priv->pdev, bar);

	switch(count) {
		case 1:
			// read byte
			*(u8 *)buffer = readb( base + offset );
			break;
		case 2:
			// read word
			if ((offset & 0x01)!=0)  {
				printk(KERN_ERR "%s rorcfs_attr_bar_read invalid offset for readw: %llx\n",
						DRV_NAME, offset);
				return -EINVAL;
			}
			*(u16 *)buffer = readw( base + offset );
			break;
		case 4:
			// read dword
			if ((offset & 0x03)!=0)  {
				printk(KERN_ERR "%s rorcfs_attr_bar_read invalid offset for readl: %llx\n",
						DRV_NAME, offset);
				return -EINVAL;
			}
			*(u32 *)buffer = readl( base + offset );
			break;

        case 1024: //make valgrid happy
                  *(u32 *)buffer = 0;
                  printk(KERN_ERR "%s rorcfs_attr_bar_read 1024 byte fake read\n", DRV_NAME);
                  break;

		default:
			printk(KERN_ERR "%s rorcfs_attr_bar_read invalid byte count: %ld\n",
					DRV_NAME, count);
			return -EINVAL;
			break;
	}

	iounmap(base); // TODO not unmaped in case of error

	return count;
}



/**
 * rorcfs_attr_bar_write: write to mapped BAR in mmap/bar[0-6]
 *
 * return number of bytes written on success, -EINVAL on error
 **/
	ssize_t rorcfs_attr_bar_write(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count)
{
	struct device *dev = 
		container_of(kobj->parent, struct device, kobj);
	struct rorcfs_priv *priv = dev_get_drvdata(dev);

	unsigned long size;
	void __iomem *base;
	int bar = -1;
	int i;

	for(i=0; i<PCI_NUM_RESOURCES; i++) {
		if ( attr == &attr_bin_bar[i] ) {
			bar = i;
			break;
		}
	}

	if ( bar==-1 ) {
		printk(KERN_ERR "%s rorcfs_attr_bar_write invalid BAR %s\n",
				DRV_NAME, attr->attr.name);
		return -EINVAL;
	}

	size =  pci_resource_len(priv->pdev, bar);

	// check if request is within the BAR
	if (offset + count >  size) {
		printk(KERN_ERR "%s rorcfs_attr_bar_write offset + count > size\n",
				DRV_NAME);
		return -EINVAL;
	}

	// get base address
	base = pci_ioremap_bar(priv->pdev, bar);

	switch(count) {
		case 1:
			// write byte
			writeb( *(u8 *)buffer, base + offset);
			break;
		case 2:
			// write word
			if ((offset & 0x01)!=0)  {
				printk(KERN_ERR "%s rorcfs_attr_bar_write invalid offset "
						"for writew: %llx\n",	DRV_NAME, offset);
				return -EINVAL;
			}
			writew( *(u16 *)buffer, base + offset);
			break;
		case 4:
			// write dword
			if ((offset & 0x03)!=0)  {
				printk(KERN_ERR "%s rorcfs_attr_bar_write invalid offset "
						"for writel: %llx\n",	DRV_NAME, offset);
				return -EINVAL;
			}
			writel( *(u32 *)buffer, base + offset);
			break;
		default:
			printk(KERN_ERR "%s rorcfs_attr_bar_write invalid byte count: %ld\n",
					DRV_NAME, count);
			return -EINVAL;
			break;
	}

	iounmap(base);

	return count;
}


/**
 * rorcfs_attr_free_buffer: free allocated buffer
 *
 * call with (unsigned long)bufid as (*buffer) argument
 **/
	ssize_t rorcfs_attr_free_buffer(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count)
{
	struct rorcfs_priv *priv;
	struct rorcfs_buf *buf;
	struct list_head *ptr;
	unsigned long bufid;
	struct device *dev = 
		container_of(kobj->parent, struct device, kobj);

	if ( offset!=0 || count!=sizeof(unsigned long) ) {
		printk(KERN_ERR "%s invalid write request to free_buffer "
				"with offset %llu, count %lu\n", DRV_NAME, offset, count);
		return -EINVAL;
	}

	memcpy(&bufid, buffer, sizeof(unsigned long));
	rorcfs_debug("%s trying to release buffer %lu\n", DRV_NAME, bufid);

	priv = pci_get_drvdata(to_pci_dev(dev));

	/* check if requested buffer exists */
	list_for_each(ptr, &priv->buf->list){
		buf = list_entry(ptr, struct rorcfs_buf, list);
		if(buf->bufid==bufid) {
			// requested buffer found -> release
			rorcfs_sysfs_buf_attr_release(buf->kobj);
			rorcfs_free_pages(buf, priv);
			list_del(ptr);
			vfree(buf);
			return count;
		}
	}

	return -EINVAL;
}


/**
 * rorcfs_attr_alloc_buffer: allocate DMA buffer
 **/
	ssize_t rorcfs_attr_alloc_buffer(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count)
{
	struct rorcfs_priv *priv;
	struct rorcfs_buf *buf;
	struct list_head *ptr;
	struct t_rorcfs_buffer buf_desc;
	int err;

	struct device *dev = 
		container_of(kobj->parent, struct device, kobj);
	priv = pci_get_drvdata(to_pci_dev(dev));

	if ( offset!=0 || count!=sizeof(struct t_rorcfs_buffer) ) {
		printk(KERN_ERR "%s attr_alloc_buffer: invalid write request "
				"with offset %llu, count %lu\n", DRV_NAME, offset, count);
		return -EINVAL;
	}

	memcpy(&buf_desc, buffer, sizeof(struct t_rorcfs_buffer));
	rorcfs_debug("%s attr_alloc_buffer: requesting 0x%lx(%lu) bytes with "
			"ID=%03lu\n",	DRV_NAME, buf_desc.bytes, buf_desc.bytes, buf_desc.id);

	if(buf_desc.bytes==0) {
		printk(KERN_ERR "%s attr_alloc_buffer: requesting zero sized "
				"buffer, aborting\n", DRV_NAME);
		return -EINVAL;
	}

	list_for_each(ptr, &priv->buf->list){
		buf = list_entry(ptr, struct rorcfs_buf, list);
		if (buf->bufid==buf_desc.id) {
			printk(KERN_ERR "%s attr_alloc_buffer: cannot create buffer with "
					"ID=%lu: already exists\n", DRV_NAME, buf_desc.id);
			return -EEXIST;
		}
	}

	buf = vmalloc(sizeof(struct rorcfs_buf));
	if( buf == NULL ) {
		printk(KERN_ERR "%s attr_alloc_buffer: failed to allocate memory "
				"for rorcfs_buf\n", DRV_NAME);
		return -ENOMEM;
	}

	// clear struct
	memset(buf, 0, sizeof(struct rorcfs_buf));

	buf->bufid = buf_desc.id;
	buf->overmap = buf_desc.overmap;

	switch (buf_desc.dma_direction) {
		case RORCFS_DMA_FROM_DEVICE :
			buf->dma_direction = DMA_FROM_DEVICE;
			break;
		case RORCFS_DMA_TO_DEVICE :
			buf->dma_direction = DMA_TO_DEVICE;
			break;
		case RORCFS_DMA_BIDIRECTIONAL :
			buf->dma_direction = DMA_BIDIRECTIONAL;
			break;
		default:
			printk(KERN_WARNING "%s unrecognized DMA direction %d, "
					"using default DMA_FROM_DEVICE\n", 
					DRV_NAME, buf_desc.dma_direction);
			buf->dma_direction = DMA_FROM_DEVICE;
			break;
	}

	if ( buf_desc.bytes & (PAGE_SIZE-1) ) // not a multiple of PAGE_SIZE
		buf->nr_pages = (buf_desc.bytes >> PAGE_SHIFT) + 1;
	else // .bytes is a multiple of PAGE_SIZE
		buf->nr_pages = (buf_desc.bytes >> PAGE_SHIFT);

	buf->sglen = 0;

	rorcfs_debug("%s requesting %ld bytes\n",	
			DRV_NAME, buf_desc.bytes);

	rorcfs_debug("%s allocating %lu pages for buffer %lu\n",
			DRV_NAME, buf->nr_pages, buf->bufid);

	// allocate memory for the DMA buffer
	err = rorcfs_alloc_pages(buf, priv);
	if (err) {
		printk(KERN_ERR "%s attr_alloc_buffer: failed to allocate pages\n", 
				DRV_NAME);
		goto err_alloc_failed;
	}

	// create sysfs entries
	err = rorcfs_sysfs_buf_attr_init(buf, priv->kobj);
	if (err) {
		printk(KERN_ERR "%s attr_alloc_buffer: failed to create rorcfs_buf "
				"specific sysfs entries\n", DRV_NAME);
		goto err_sysfs_attrs_failed;
	}

	// add new list element
	list_add_tail(&buf->list, &priv->buf->list);

	return count;

err_sysfs_attrs_failed:
	rorcfs_sysfs_buf_attr_release(buf->kobj);
	rorcfs_free_pages(buf, priv);

err_alloc_failed:
	vfree(buf);

	return err;
}



/**************************************************
 * helper functions to batch process bin attributes
 **************************************************/

/**
 * rorcfs_sysfs_create_bin_files
 **/
int rorcfs_sysfs_create_bin_files(struct kobject *kobj, 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
		const 
#endif
		struct bin_attribute **ptr)
{
	int err = 0;
	int i;

	for (i=0; ptr[i] && !err; i++)
		err = sysfs_create_bin_file(kobj, ptr[i]);
	if (err)
		while (--i >= 0)
			sysfs_remove_bin_file(kobj, ptr[i]);
	return err;
}

/**
 * rorcfs_sysfs_remove_bin_files
 **/
void rorcfs_sysfs_remove_bin_files(struct kobject *kobj, 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
		const 
#endif
		struct bin_attribute **ptr)
{
	int i;
	for (i = 0; ptr[i]; i++)
		sysfs_remove_bin_file(kobj, ptr[i]);
}
