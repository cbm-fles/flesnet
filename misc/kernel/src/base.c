/**
 * @file base.c
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
#include <linux/mm.h>
#include <asm/page.h>
#include <linux/list.h>
#include <linux/pagemap.h>
#include <linux/kobject.h>
#include <linux/scatterlist.h>

#include "base.h"
#include "buffer.h"
#include "sysfs.h"
#include "../include/rorcfs.h"

static BIN_ATTR(free_buffer, sizeof(unsigned long), S_IWUGO, NULL, rorcfs_attr_free_buffer, NULL);
static BIN_ATTR(alloc_buffer, sizeof(struct t_rorcfs_buffer), S_IWUGO, NULL, rorcfs_attr_alloc_buffer, NULL);

/**
 * sysfs attribute for each possible bar
 * attribute is only created if corresponding BAR actually exists
 * */
struct bin_attribute attr_bin_bar[PCI_NUM_RESOURCES] = {
	BIN_ATTR_CORE(bar0, 0, S_IRUGO|S_IWUGO, rorcfs_attr_bar_read, 
		rorcfs_attr_bar_write, rorcfs_bar_mmap),
	BIN_ATTR_CORE(bar1, 0, S_IRUGO|S_IWUGO, NULL, 
		rorcfs_attr_bar_write, rorcfs_bar_mmap),
	BIN_ATTR_CORE(bar2, 0, S_IRUGO|S_IWUGO, rorcfs_attr_bar_read, 
		rorcfs_attr_bar_write, rorcfs_bar_mmap),
	BIN_ATTR_CORE(bar3, 0, S_IRUGO|S_IWUGO, rorcfs_attr_bar_read, 
		rorcfs_attr_bar_write, rorcfs_bar_mmap),
	BIN_ATTR_CORE(bar4, 0, S_IRUGO|S_IWUGO, rorcfs_attr_bar_read, 
		rorcfs_attr_bar_write, rorcfs_bar_mmap),
	BIN_ATTR_CORE(bar5, 0, S_IRUGO|S_IWUGO, rorcfs_attr_bar_read, 
		rorcfs_attr_bar_write, rorcfs_bar_mmap),
};


/**
 * rorcfs_probe: Device Initialization Routine
 * @pdev: PCI device information struct
 * @id: entry in rorcfs_pci_table
 *
 * Returns 0 on success, negative on failure
 **/
static int rorcfs_init_one(struct pci_dev *pdev, 
		const struct pci_device_id *id)
{
	int err, i;
	struct rorcfs_priv *priv;
	struct rorcfs_buf *buf;
	struct resource *r;

	// allocate memory for the private struct
	priv = kmalloc(sizeof(struct rorcfs_priv), GFP_KERNEL);
	buf = (struct rorcfs_buf *)kmalloc(sizeof(struct rorcfs_buf), GFP_KERNEL);

	/* enable device */
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot enable PCI device, "
				"aborting\n");
		return err;
	}
	dev_info(&pdev->dev, "Device enabled");

	/* request regions */
	err = pci_request_regions(pdev, DRV_NAME);
	if (err) {
		dev_err(&pdev->dev, "Couldn't get PCI resources, "
				"aborting\n");
		goto err_disable_pdev;
	}

	/* set master */
	pci_set_master(pdev);

	/* save state */
	err = pci_save_state(pdev);
	if (err) {
		dev_err(&pdev->dev, "PCI save state failed, "
				"aborting\n");
		goto err_release_regions;
	}

	/* set DMA mask */
	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit PCI DMA mask.\n");
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "Can't set PCI DMA mask, aborting\n");
			goto err_release_regions;
		}
	}

	/* set consistent DMA mask */
	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit "
				"consistent PCI DMA mask.\n");
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "Can't set consistent "
					"PCI DMA mask, aborting\n");
			goto err_release_regions;
		}
	}

	// assign pdev to private struct
	priv->pdev = pdev;

	INIT_LIST_HEAD(&buf->list);
	priv->buf = buf;

	// save private struct as driver private data
	// can be fetched again with pci_get_drvdata(struct pci_dev *pdev);
	pci_set_drvdata(pdev, priv);

	// create sysfs "mmap" subdirectory under "device"
	priv->kobj = kobject_create_and_add("mmap", &pdev->dev.kobj);
	if (!priv->kobj) {
		err = -ENOMEM;
		goto err_remove_file;
	}

	// create attribute "alloc_buffer" in subdirectory
	err = sysfs_create_bin_file(priv->kobj, &attr_bin_alloc_buffer);
	if (err) {
		dev_err(&pdev->dev, "Can't create free_buffer sysfs entry\n");
		goto err_kobj_create;
	}

	// create attribute "free_buffer" in subdirectory
	err = sysfs_create_bin_file(priv->kobj, &attr_bin_free_buffer);
	if (err) {
		dev_err(&pdev->dev, "Can't create free_buffer sysfs entry\n");
		goto err_release_alloc_buffer;
	}

	// create attributes for BAR mappings
	for (i=0; i< PCI_NUM_RESOURCES; i++) {
		r = &pdev->resource[i];
		if ( !r->flags || !(r->flags & IORESOURCE_MEM) )
			continue;

		attr_bin_bar[i].size = pci_resource_end(priv->pdev, i) - 
			pci_resource_start(priv->pdev, i);

		err = sysfs_create_bin_file(priv->kobj, &attr_bin_bar[i]);	
		if (err) {
			dev_err(&pdev->dev, "Failed to create sysfs entry for BAR%d\n", i);
			goto err_bar_create;
		}
	}

	return 0;

err_bar_create:
	for (i=0; i < PCI_NUM_RESOURCES; i++ ) {
		if ( !r->flags || !(r->flags & IORESOURCE_MEM) )
			continue;
		sysfs_remove_bin_file(priv->kobj, &attr_bin_bar[i]);
	}
	sysfs_remove_bin_file(priv->kobj, &attr_bin_free_buffer);

err_release_alloc_buffer:
	sysfs_remove_bin_file(priv->kobj, &attr_bin_alloc_buffer);

err_kobj_create:
	kobject_put(priv->kobj);

err_remove_file:
	kfree(buf);

err_release_regions:
	pci_release_regions(pdev);

err_disable_pdev:
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	return err;
}


/**
 * rorcfs_remove_one: remove device, unmap & free buffers
 * @pdev: pointer to pci_dev struct
 **/
static void rorcfs_remove_one(struct pci_dev *pdev)
{
	struct rorcfs_priv *priv;
	struct list_head *ptr, *q;
	struct rorcfs_buf *tmp;
	struct resource *r;
	int i;
	priv = pci_get_drvdata(pdev);

	/* iterate over buffer list */
	list_for_each_safe(ptr, q, &priv->buf->list) {
		tmp = list_entry(ptr, struct rorcfs_buf, list);
		/* release sysfs entries */
		rorcfs_sysfs_buf_attr_release(tmp->kobj);
		/* unmap buffers */
		rorcfs_free_pages(tmp, priv);
		list_del(ptr);
		vfree(tmp);
	}

	// remove binfile "free_buffer"
	sysfs_remove_bin_file(priv->kobj, &attr_bin_free_buffer);
	// remove binfile "alloc_buffer"
	sysfs_remove_bin_file(priv->kobj, &attr_bin_alloc_buffer);

	// remove BAR mappings "mmap/barX"
	for (i=0; i < PCI_NUM_RESOURCES; i++ ) {
		r = &pdev->resource[i];
		if ( !r->flags || !(r->flags & IORESOURCE_MEM) )
			continue;
		sysfs_remove_bin_file(priv->kobj, &attr_bin_bar[i]);
	}

	// release "mmap"
	kobject_put(priv->kobj);

	pci_release_regions(pdev);
	pci_disable_device(pdev);
}


/**
 * PCI Device Table
 * add any device/vendor-id here you want to get bound to rorfs
 * */
static DEFINE_PCI_DEVICE_TABLE(rorcfs_pci_table) = {
  //	{ PCI_DEVICE(0x10ee, 0x6024) }, /* ML605 PCIe RefDesign, V6 ES */
  //	{ PCI_DEVICE(0x10ee, 0x6028) }, /* HTG-PCIe Board, V6 Prod. */
        //	{ PCI_DEVICE(0x8086, 0x1c22) }, /* TESTING: Intel Corporation Cougar Point SMBus Controller */
        //	{ PCI_DEVICE(0x8086, 0x3b30) }, /* TESTING: SMBus: Intel Corporation Ibex Peak SMBus Controller */
        //	{ PCI_DEVICE(0x8086, 0x1c3a) }, /* TESTING: Communication controller: Intel Corporation Cougar Point HECI Controller #1 */
  //	{ PCI_DEVICE(0x8086, 0x3a30) }, /* TESTING: Intel Corporation 82801JI (ICH10 Family) SMBus Controller*/
	{ PCI_DEVICE(0x10dc, 0xbeaf) }, /* CERN PCI-ID, Device-ID=0xbeaf */
	{ 0, }
};

static struct pci_driver rorcfs_driver = {
	.name			= DRV_NAME,
	.id_table	= rorcfs_pci_table,
	.probe		= rorcfs_init_one,
	.remove		= __devexit_p(rorcfs_remove_one)
};

static int __init rorcfs_init(void)
{
	int ret;
	ret = pci_register_driver(&rorcfs_driver);
	return ret < 0 ? ret : 0;
}

static void __exit rorcfs_exit(void)
{
	pci_unregister_driver(&rorcfs_driver);
	rorcfs_debug("%s: exit()\n", DRV_NAME);
}

module_init(rorcfs_init);
module_exit(rorcfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RORC SysFS Driver");
MODULE_AUTHOR("hengel@cern.ch");
MODULE_VERSION("0.0.1");


