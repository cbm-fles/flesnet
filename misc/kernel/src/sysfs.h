/**
 * @file sysfs.h
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

#ifndef _RORCFS_SYSFS_H
#define _RORCFS_SYSFS_H

#define BIN_ATTR(_name, _size, _mode, _read, _write, _mmap) \
	struct bin_attribute attr_bin_##_name = { \
		.attr = { .name = __stringify(_name), .mode = _mode }, \
		.size = _size, \
		.read = _read, \
		.write = _write, \
		.mmap = _mmap, \
	}

#define BIN_ATTR_CORE(_name, _size, _mode, _read, _write, _mmap) \
{ \
	.attr = { .name = __stringify(_name), .mode = _mode }, \
	.size = _size, \
	.read = _read, \
	.write = _write, \
	.mmap = _mmap, \
}



int rorcfs_sysfs_buf_attr_init(struct rorcfs_buf *buf, 
		struct kobject *kobj_parent);

void rorcfs_sysfs_buf_attr_release(struct kobject *kobj);

ssize_t attr_alloc_buffer_show(struct device *dev, 
		struct device_attribute *attr, char *buffer);

ssize_t attr_alloc_buffer_store(struct device *dev, 
		struct device_attribute *attr, 
		const char *buffer, size_t count);

	ssize_t rorcfs_attr_bin_read(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp,
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count);

	ssize_t rorcfs_attr_bin_write(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count);

	int rorcfs_attr_bin_mmap(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj, 
			struct bin_attribute *attr, struct vm_area_struct *vma);

	int rorcfs_mmap_virtual(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj, 
			struct bin_attribute *attr, struct vm_area_struct *vma);


// BAR access 
void rorcfs_bar_vma_open(struct vm_area_struct *vma);
void rorcfs_bar_vma_close(struct vm_area_struct *vma);
	int rorcfs_bar_mmap(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj, 
			struct bin_attribute *attr, struct vm_area_struct *vma);

	ssize_t rorcfs_attr_bar_read(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count);

	ssize_t rorcfs_attr_bar_write(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp,
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count);

	ssize_t rorcfs_attr_free_buffer(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp, 
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count);

	ssize_t rorcfs_attr_alloc_buffer(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
			struct file *filp,
#endif
			struct kobject *kobj,
			struct bin_attribute *attr, 
			char *buffer, loff_t offset, size_t count);


/* helper functions */
int rorcfs_sysfs_create_bin_files(struct kobject *kobj, 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
		const 
#endif
		struct bin_attribute **ptr);

void rorcfs_sysfs_remove_bin_files(struct kobject *kobj, 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
		const 
#endif
		struct bin_attribute **ptr);
#endif
