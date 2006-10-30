/*
 *  History:
 *  Started: October 2006 by Paul Betts, to allow user process control 
 *  	     of Object Storage Devices.
 *
 * Original driver (suo.c):
 *        Copyright (C) 2006 Paul Betts <bettsp@osc.edu>
 *        Based on sg and sd driver originally by Lawrence Foard and 
 *        	Drew Eckhardt respectively.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

/*#include <linux/config.h>*/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsicam.h>

#include "scsi_logging.h"

/* OSD Includes */
#include "../libosd/osd_defs.h"

/*
 * Constants
 */

#define SUO_MAX_OSD_ENTRIES 	64
#define SUO_DEVICE_PREFIX 	"su"

MODULE_AUTHOR("Paul Betts");
MODULE_DESCRIPTION("SCSI user-mode object storage (suo) driver");
MODULE_LICENSE("GPL");


static int major;
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");


/*
 * This is limited by the naming scheme enforced in suo_probe,
 * add another character to it if you really need more disks.
 */
#define SD_MAX_DISKS	(((26 * 26) + 26 + 1) * 26)

/*
 * Time out in seconds for disks and Magneto-opticals (which are slower).
 */
#define SD_TIMEOUT		(30 * HZ)
#define SD_MOD_TIMEOUT		(75 * HZ)

/*
 * Number of allowed retries
 */
#define SD_MAX_RETRIES		5
#define SD_PASSTHROUGH_RETRIES	1

/*
 * Size of the initial data buffer for mode and read capacity data
 */
#define SD_BUF_SIZE		512


/*
 * Globals
 */
static struct class *so_sysfs_class;
static dev_t dev_id;

/*
 * Structs
 */
struct ohd_struct {
	struct kobject kobj;
	struct kobject *holder_dir;
	unsigned ios[2], sectors[2];	/* READs and WRITEs */
	int policy, partno;
};

struct genosddisk {
	int major;			/* major number of driver */
	int first_minor;
	int minors;                     /* maximum number of minors, =1 for
                                         * disks that can't be partitioned. */
	char disk_name[32];		/* name of major driver */
	struct hd_struct **part;	/* [indexed by minor] */
	int part_uevent_suppress;
	struct file_operations *fops;
	struct request_queue *queue;
	void *private_data;
	sector_t capacity;

	int flags;
	struct device *driverfs_dev;
	struct kobject kobj;
	struct kobject *holder_dir;
	struct kobject *slave_dir;

	struct timer_rand_state *random;
	int policy;

	atomic_t sync_io;		/* RAID */
	unsigned long stamp;
	int in_flight;
#ifdef	CONFIG_SMP
	struct disk_stats *dkstats;
#else
	struct disk_stats dkstats;
#endif
};

struct scsi_osd_disk {
	struct scsi_driver *driver;	/* always &suo_template */
	struct scsi_device *device;
	struct class_device cdev;
	struct genosddisk *disk;
	unsigned int	openers;	/* protected by BKL for now, yuck */
	sector_t	capacity;	/* size in 512-byte sectors */
	u32		index;
	u8		media_present;
	u8		write_prot;
	unsigned	WCE : 1;	/* state of disk WCE bit */
	unsigned	RCD : 1;	/* state of disk RCD bit, unused */
	unsigned	DPOFUA : 1;	/* state of disk DPOFUA bit */
};
#define to_osd_disk(obj) container_of(obj,struct scsi_osd_disk,cdev)

static DEFINE_IDR(sd_index_idr);
static DEFINE_SPINLOCK(sd_index_lock);

/* This semaphore is used to mediate the 0->1 reference get in the
 * face of object destruction (i.e. we can't allow a get on an
 * object after last put) */
static DEFINE_MUTEX(sd_ref_mutex);

static int suo_revalidate_disk(struct genosddisk *disk);
static void sd_rw_intr(struct scsi_cmnd * command);
static int suo_probe(struct device *);
static int suo_remove(struct device *);
static void sd_shutdown(struct device *dev);
static void sd_rescan(struct device *);
static int sd_init_command(struct scsi_cmnd *);
static int sd_issue_flush(struct device *, sector_t *);
static void sd_prepare_flush(request_queue_t *, struct request *);
static void suo_read_capacity(struct scsi_osd_disk *sdkp, char *diskname,
			     unsigned char *buffer);
static void scsi_osd_disk_release(struct class_device *cdev);
static int sd_ioctl(struct inode * inode, struct file * filp, 
		    unsigned int cmd, unsigned long arg);

static const char *sd_cache_types[] = {
	"write through", "none", "write back",
	"write back, no read (daft)"
};

static ssize_t sd_store_cache_type(struct class_device *cdev, const char *buf,
				   size_t count)
{
	int i, ct = -1, rcd, wce, sp;
	struct scsi_osd_disk *sdkp = to_osd_disk(cdev);
	struct scsi_device *sdp = sdkp->device;
	char buffer[64];
	char *buffer_data;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;
	int len;

	if (sdp->type != TYPE_OSD || sdp->type != TYPE_DISK)
		/* no cache control on RBC devices; theoretically they
		 * can do it, but there's probably so many exceptions
		 * it's not worth the risk */
		return -EINVAL;

	for (i = 0; i < sizeof(sd_cache_types)/sizeof(sd_cache_types[0]); i++) {
		const int len = strlen(sd_cache_types[i]);
		if (strncmp(sd_cache_types[i], buf, len) == 0 &&
		    buf[len] == '\n') {
			ct = i;
			break;
		}
	}
	if (ct < 0)
		return -EINVAL;
	rcd = ct & 0x01 ? 1 : 0;
	wce = ct & 0x02 ? 1 : 0;
	if (scsi_mode_sense(sdp, 0x08, 8, buffer, sizeof(buffer), SD_TIMEOUT,
			    SD_MAX_RETRIES, &data, NULL))
		return -EINVAL;
	len = min_t(size_t, sizeof(buffer), data.length - data.header_length -
		  data.block_descriptor_length);
	buffer_data = buffer + data.header_length +
		data.block_descriptor_length;
	buffer_data[2] &= ~0x05;
	buffer_data[2] |= wce << 2 | rcd;
	sp = buffer_data[0] & 0x80 ? 1 : 0;

	if (scsi_mode_select(sdp, 1, sp, 8, buffer_data, len, SD_TIMEOUT,
			     SD_MAX_RETRIES, &data, &sshdr)) {
		if (scsi_sense_valid(&sshdr))
			scsi_print_sense_hdr(sdkp->disk->disk_name, &sshdr);
		return -EINVAL;
	}
	suo_revalidate_disk(sdkp->disk);
	return count;
}

static ssize_t sd_show_cache_type(struct class_device *cdev, char *buf)
{
	struct scsi_osd_disk *sdkp = to_osd_disk(cdev);
	int ct = sdkp->RCD + 2*sdkp->WCE;

	return snprintf(buf, 40, "%s\n", sd_cache_types[ct]);
}

static ssize_t sd_show_fua(struct class_device *cdev, char *buf)
{
	struct scsi_osd_disk *sdkp = to_osd_disk(cdev);

	return snprintf(buf, 20, "%u\n", sdkp->DPOFUA);
}

static struct class_device_attribute suo_disk_attrs[] = {
	__ATTR(cache_type, S_IRUGO|S_IWUSR, sd_show_cache_type,
	       sd_store_cache_type),
	__ATTR(FUA, S_IRUGO, sd_show_fua, NULL),
	__ATTR_NULL,
};

static struct class suo_disk_class = {
	.name		= "scsi_osd",
	.owner		= THIS_MODULE,
	.release	= scsi_osd_disk_release,
	.class_dev_attrs = suo_disk_attrs,
};

static struct scsi_driver suo_template = {
	.owner			= THIS_MODULE,
	.gendrv = {
		.name		= "suo",
		.probe		= suo_probe,
		.remove		= suo_remove,
		.shutdown	= sd_shutdown,
	},
	.rescan			= sd_rescan,
	.init_command		= sd_init_command,
	.issue_flush		= sd_issue_flush,
};

static inline struct scsi_osd_disk *scsi_osd_disk(struct gendisk *disk)
{
	return container_of(disk->private_data, struct scsi_osd_disk, driver);
}

static struct scsi_osd_disk *__scsi_osd_disk_get(struct gendisk *disk)
{
	struct scsi_osd_disk *sdkp = NULL;

	if (disk->private_data) {
		sdkp = scsi_osd_disk(disk);
		if (scsi_device_get(sdkp->device) == 0)
			class_device_get(&sdkp->cdev);
		else
			sdkp = NULL;
	}
	return sdkp;
}

static struct scsi_osd_disk *scsi_osd_disk_get(struct gendisk *disk)
{
	struct scsi_osd_disk *sdkp;

	mutex_lock(&sd_ref_mutex);
	sdkp = __scsi_osd_disk_get(disk);
	mutex_unlock(&sd_ref_mutex);
	return sdkp;
}

static struct scsi_osd_disk *scsi_osd_disk_get_from_dev(struct device *dev)
{
	struct scsi_osd_disk *sdkp;

	mutex_lock(&sd_ref_mutex);
	sdkp = dev_get_drvdata(dev);
	if (sdkp)
		sdkp = __scsi_osd_disk_get(sdkp->disk);
	mutex_unlock(&sd_ref_mutex);
	return sdkp;
}

static void scsi_osd_disk_put(struct scsi_osd_disk *sdkp)
{
	struct scsi_device *sdev = sdkp->device;

	mutex_lock(&sd_ref_mutex);
	class_device_put(&sdkp->cdev);
	scsi_device_put(sdev);
	mutex_unlock(&sd_ref_mutex);
}


/*
 * gendisk stuff modified for OSD
 */

/* Inlines to alloc and free disk stats in struct genosddisk */
#ifdef  CONFIG_SMP
static inline int init_osd_disk_stats(struct genosddisk *disk)
{
	disk->dkstats = alloc_percpu(struct disk_stats);
	if (!disk->dkstats)
		return 0;
	return 1;
}

static inline void free_osd_disk_stats(struct genosddisk *disk)
{
	free_percpu(disk->dkstats);
}
#else	/* CONFIG_SMP */
static inline int init_osd_disk_stats(struct genosddisk *disk)
{
	return 1;
}

static inline void free_osd_disk_stats(struct genosddisk *disk)
{
}
#endif	/* CONFIG_SMP */

struct genosddisk *alloc_osd_disk_node(int node_id)
{
	struct genosddisk *disk;

	disk = kmalloc_node(sizeof(struct genosddisk), GFP_KERNEL, node_id);
	if (disk) {
		memset(disk, 0, sizeof(struct genosddisk));
		if (!init_osd_disk_stats(disk)) {
			kfree(disk);
			return NULL;
		}
		disk->minors = 1;
		/*kobj_set_kset_s(disk,block_subsys);*/
		kobject_init(&disk->kobj);
		/* rand_initialize_disk(disk); */ 
	}
	return disk;
}

struct genosddisk *alloc_osd_disk(void)
{
	return alloc_osd_disk_node(-1);
}

EXPORT_SYMBOL(alloc_osd_disk);
EXPORT_SYMBOL(alloc_osd_disk_node);

struct kobject *get_osd_disk(struct genosddisk *disk)
{
	struct module *owner;
	struct kobject *kobj;

	if (!disk->fops)
		return NULL;
	owner = disk->fops->owner;
	if (owner && !try_module_get(owner))
		return NULL;
	kobj = kobject_get(&disk->kobj);
	if (kobj == NULL) {
		module_put(owner);
		return NULL;
	}
	return kobj;

}
EXPORT_SYMBOL(get_osd_disk);

void put_osd_disk(struct genosddisk *disk)
{
	if (disk)
		kobject_put(&disk->kobj);
}
EXPORT_SYMBOL(put_osd_disk);



/**
 *	sd_init_command - build a scsi (read or write) command from
 *	information in the request structure.
 *	@SCpnt: pointer to mid-level's per scsi command structure that
 *	contains request and into which the scsi command is written
 *
 *	Returns 1 if successful and 0 if error (or cannot be done now).
 **/
static int sd_init_command(struct scsi_cmnd * SCpnt)
{
	struct scsi_device *sdp = SCpnt->device;
	struct request *rq = SCpnt->request;
	struct gendisk *disk = rq->rq_disk;
	sector_t block = rq->sector;
	unsigned int this_count = SCpnt->request_bufflen >> 9;
	unsigned int timeout = sdp->timeout;

	SCSI_LOG_HLQUEUE(1, printk("sd_init_command: disk=%s, block=%llu, "
			    "count=%d\n", disk->disk_name,
			 (unsigned long long)block, this_count));

	if (!sdp || !scsi_device_online(sdp) ||
 	    block + rq->nr_sectors > get_capacity(disk)) {
		SCSI_LOG_HLQUEUE(2, printk("Finishing %ld sectors\n", 
				 rq->nr_sectors));
		SCSI_LOG_HLQUEUE(2, printk("Retry with 0x%p\n", SCpnt));
		return 0;
	}

	if (sdp->changed) {
		/*
		 * quietly refuse to do anything to a changed disc until 
		 * the changed bit has been reset
		 */
		/* printk("SCSI disk has been changed. Prohibiting further I/O.\n"); */
		return 0;
	}
	SCSI_LOG_HLQUEUE(2, printk("%s : block=%llu\n",
				   disk->disk_name, (unsigned long long)block));

	/*
	 * If we have a 1K hardware sectorsize, prevent access to single
	 * 512 byte sectors.  In theory we could handle this - in fact
	 * the scsi cdrom driver must be able to handle this because
	 * we typically use 1K blocksizes, and cdroms typically have
	 * 2K hardware sectorsizes.  Of course, things are simpler
	 * with the cdrom, since it is read-only.  For performance
	 * reasons, the filesystems should be able to handle this
	 * and not force the scsi disk driver to use bounce buffers
	 * for this.
	 */
	if (sdp->sector_size == 1024) {
		if ((block & 1) || (rq->nr_sectors & 1)) {
			printk(KERN_ERR "sd: Bad block number requested");
			return 0;
		} else {
			block = block >> 1;
			this_count = this_count >> 1;
		}
	}
	if (sdp->sector_size == 2048) {
		if ((block & 3) || (rq->nr_sectors & 3)) {
			printk(KERN_ERR "sd: Bad block number requested");
			return 0;
		} else {
			block = block >> 2;
			this_count = this_count >> 2;
		}
	}
	if (sdp->sector_size == 4096) {
		if ((block & 7) || (rq->nr_sectors & 7)) {
			printk(KERN_ERR "sd: Bad block number requested");
			return 0;
		} else {
			block = block >> 3;
			this_count = this_count >> 3;
		}
	}
	if (rq_data_dir(rq) == WRITE) {
		if (!sdp->writeable) {
			return 0;
		}
		SCpnt->cmnd[0] = WRITE_6;
		SCpnt->sc_data_direction = DMA_TO_DEVICE;
	} else if (rq_data_dir(rq) == READ) {
		SCpnt->cmnd[0] = READ_6;
		SCpnt->sc_data_direction = DMA_FROM_DEVICE;
	} else {
		printk(KERN_ERR "sd: Unknown command %x\n", rq->cmd_flags);
		return 0;
	}

	SCSI_LOG_HLQUEUE(2, printk("%s : %s %d/%ld 512 byte blocks.\n", 
		disk->disk_name, (rq_data_dir(rq) == WRITE) ? 
		"writing" : "reading", this_count, rq->nr_sectors));

	SCpnt->cmnd[1] = 0;
	
	if (block > 0xffffffff) {
		SCpnt->cmnd[0] += READ_16 - READ_6;
		SCpnt->cmnd[1] |= blk_fua_rq(rq) ? 0x8 : 0;
		SCpnt->cmnd[2] = sizeof(block) > 4 ? (unsigned char) (block >> 56) & 0xff : 0;
		SCpnt->cmnd[3] = sizeof(block) > 4 ? (unsigned char) (block >> 48) & 0xff : 0;
		SCpnt->cmnd[4] = sizeof(block) > 4 ? (unsigned char) (block >> 40) & 0xff : 0;
		SCpnt->cmnd[5] = sizeof(block) > 4 ? (unsigned char) (block >> 32) & 0xff : 0;
		SCpnt->cmnd[6] = (unsigned char) (block >> 24) & 0xff;
		SCpnt->cmnd[7] = (unsigned char) (block >> 16) & 0xff;
		SCpnt->cmnd[8] = (unsigned char) (block >> 8) & 0xff;
		SCpnt->cmnd[9] = (unsigned char) block & 0xff;
		SCpnt->cmnd[10] = (unsigned char) (this_count >> 24) & 0xff;
		SCpnt->cmnd[11] = (unsigned char) (this_count >> 16) & 0xff;
		SCpnt->cmnd[12] = (unsigned char) (this_count >> 8) & 0xff;
		SCpnt->cmnd[13] = (unsigned char) this_count & 0xff;
		SCpnt->cmnd[14] = SCpnt->cmnd[15] = 0;
	} else if ((this_count > 0xff) || (block > 0x1fffff) ||
		   SCpnt->device->use_10_for_rw) {
		if (this_count > 0xffff)
			this_count = 0xffff;

		SCpnt->cmnd[0] += READ_10 - READ_6;
		SCpnt->cmnd[1] |= blk_fua_rq(rq) ? 0x8 : 0;
		SCpnt->cmnd[2] = (unsigned char) (block >> 24) & 0xff;
		SCpnt->cmnd[3] = (unsigned char) (block >> 16) & 0xff;
		SCpnt->cmnd[4] = (unsigned char) (block >> 8) & 0xff;
		SCpnt->cmnd[5] = (unsigned char) block & 0xff;
		SCpnt->cmnd[6] = SCpnt->cmnd[9] = 0;
		SCpnt->cmnd[7] = (unsigned char) (this_count >> 8) & 0xff;
		SCpnt->cmnd[8] = (unsigned char) this_count & 0xff;
	} else {
		if (unlikely(blk_fua_rq(rq))) {
			/*
			 * This happens only if this drive failed
			 * 10byte rw command with ILLEGAL_REQUEST
			 * during operation and thus turned off
			 * use_10_for_rw.
			 */
			printk(KERN_ERR "sd: FUA write on READ/WRITE(6) drive\n");
			return 0;
		}

		SCpnt->cmnd[1] |= (unsigned char) ((block >> 16) & 0x1f);
		SCpnt->cmnd[2] = (unsigned char) ((block >> 8) & 0xff);
		SCpnt->cmnd[3] = (unsigned char) block & 0xff;
		SCpnt->cmnd[4] = (unsigned char) this_count;
		SCpnt->cmnd[5] = 0;
	}
	SCpnt->request_bufflen = this_count * sdp->sector_size;

	/*
	 * We shouldn't disconnect in the middle of a sector, so with a dumb
	 * host adapter, it's safe to assume that we can at least transfer
	 * this many bytes between each connect / disconnect.
	 */
	SCpnt->transfersize = sdp->sector_size;
	SCpnt->underflow = this_count << 9;
	SCpnt->allowed = SD_MAX_RETRIES;
	SCpnt->timeout_per_command = timeout;

	/*
	 * This is the completion routine we use.  This is matched in terms
	 * of capability to this function.
	 */
	SCpnt->done = sd_rw_intr;

	/*
	 * This indicates that the command is ready from our end to be
	 * queued.
	 */
	return 1;
}

/**
 *	sd_open - open a scsi disk device
 *	@inode: only i_rdev member may be used
 *	@filp: only f_mode and f_flags may be used
 *
 *	Returns 0 if successful. Returns a negated errno value in case 
 *	of error.
 *
 *	Note: This can be called from a user context (e.g. fsck(1) )
 *	or from within the kernel (e.g. as a result of a mount(1) ).
 *	In the latter case @inode and @filp carry an abridged amount
 *	of information as noted above.
 **/
static int sd_open(struct inode *inode, struct file *filp)
{
	struct gendisk *disk = inode->i_bdev->bd_disk;
	struct scsi_osd_disk *sdkp;
	struct scsi_device *sdev;
	int retval;

	if (!(sdkp = scsi_osd_disk_get(disk)))
		return -ENXIO;


	SCSI_LOG_HLQUEUE(3, printk("sd_open: disk=%s\n", disk->disk_name));

	sdev = sdkp->device;

	/*
	 * If the device is in error recovery, wait until it is done.
	 * If the device is offline, then disallow any access to it.
	 */
	retval = -ENXIO;
	if (!scsi_block_when_processing_errors(sdev))
		goto error_out;

	if (sdev->removable || sdkp->write_prot)
		check_disk_change(inode->i_bdev);

	/*
	 * If the drive is empty, just let the open fail.
	 */
	retval = -ENOMEDIUM;
	if (sdev->removable && !sdkp->media_present &&
	    !(filp->f_flags & O_NDELAY))
		goto error_out;

	/*
	 * If the device has the write protect tab set, have the open fail
	 * if the user expects to be able to write to the thing.
	 */
	retval = -EROFS;
	if (sdkp->write_prot && (filp->f_mode & FMODE_WRITE))
		goto error_out;

	/*
	 * It is possible that the disk changing stuff resulted in
	 * the device being taken offline.  If this is the case,
	 * report this to the user, and don't pretend that the
	 * open actually succeeded.
	 */
	retval = -ENXIO;
	if (!scsi_device_online(sdev))
		goto error_out;

	if (!sdkp->openers++ && sdev->removable) {
		if (scsi_block_when_processing_errors(sdev))
			scsi_set_medium_removal(sdev, SCSI_REMOVAL_PREVENT);
	}

	return 0;

error_out:
	scsi_osd_disk_put(sdkp);
	return retval;	
}

/**
 *	sd_release - invoked when the (last) close(2) is called on this
 *	scsi disk.
 *	@inode: only i_rdev member may be used
 *	@filp: only f_mode and f_flags may be used
 *
 *	Returns 0. 
 *
 *	Note: may block (uninterruptible) if error recovery is underway
 *	on this disk.
 **/
static int sd_release(struct inode *inode, struct file *filp)
{
	struct gendisk *disk = inode->i_bdev->bd_disk;
	struct scsi_osd_disk *sdkp = scsi_osd_disk(disk);
	struct scsi_device *sdev = sdkp->device;

	SCSI_LOG_HLQUEUE(3, printk("sd_release: disk=%s\n", disk->disk_name));

	if (!--sdkp->openers && sdev->removable) {
		if (scsi_block_when_processing_errors(sdev))
			scsi_set_medium_removal(sdev, SCSI_REMOVAL_ALLOW);
	}

	/*
	 * XXX and what if there are packets in flight and this close()
	 * XXX is followed by a "rmmod suo_mod"?
	 */
	scsi_osd_disk_put(sdkp);
	return 0;
}

static int sd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct scsi_osd_disk *sdkp = scsi_osd_disk(bdev->bd_disk);
	struct scsi_device *sdp = sdkp->device;
	struct Scsi_Host *host = sdp->host;
	int diskinfo[4];

	/* default to most commonly used values */
        diskinfo[0] = 0x40;	/* 1 << 6 */
       	diskinfo[1] = 0x20;	/* 1 << 5 */
       	diskinfo[2] = sdkp->capacity >> 11;
	
	/* override with calculated, extended default, or driver values */
	if (host->hostt->bios_param)
		host->hostt->bios_param(sdp, bdev, sdkp->capacity, diskinfo);
	else
		scsicam_bios_param(bdev, sdkp->capacity, diskinfo);

	geo->heads = diskinfo[0];
	geo->sectors = diskinfo[1];
	geo->cylinders = diskinfo[2];
	return 0;
}

/**
 *	sd_ioctl - process an ioctl
 *	@inode: only i_rdev/i_bdev members may be used
 *	@filp: only f_mode and f_flags may be used
 *	@cmd: ioctl command number
 *	@arg: this is third argument given to ioctl(2) system call.
 *	Often contains a pointer.
 *
 *	Returns 0 if successful (some ioctls return postive numbers on
 *	success as well). Returns a negated errno value in case of error.
 *
 *	Note: most ioctls are forward onto the block subsystem or further
 *	down in the scsi subsytem.
 **/
static int sd_ioctl(struct inode * inode, struct file * filp, 
		    unsigned int cmd, unsigned long arg)
{
	struct block_device *bdev = inode->i_bdev;
	struct gendisk *disk = bdev->bd_disk;
	struct scsi_device *sdp = scsi_osd_disk(disk)->device;
	void __user *p = (void __user *)arg;
	int error;
    
	SCSI_LOG_IOCTL(1, printk("sd_ioctl: disk=%s, cmd=0x%x\n",
						disk->disk_name, cmd));

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	error = scsi_nonblockable_ioctl(sdp, cmd, p, filp);
	if (!scsi_block_when_processing_errors(sdp) || !error)
		return error;

	/*
	 * Send SCSI addressing ioctls directly to mid level, send other
	 * ioctls to block level and then onto mid level if they can't be
	 * resolved.
	 */
	switch (cmd) {
		case SCSI_IOCTL_GET_IDLUN:
		case SCSI_IOCTL_GET_BUS_NUMBER:
			return scsi_ioctl(sdp, cmd, p);
		default:
			error = scsi_cmd_ioctl(filp, disk, cmd, p);
			if (error != -ENOTTY)
				return error;
	}
	return scsi_ioctl(sdp, cmd, p);
}

static void set_media_not_present(struct scsi_osd_disk *sdkp)
{
	sdkp->media_present = 0;
	sdkp->capacity = 0;
	sdkp->device->changed = 1;
}

/**
 *	sd_media_changed - check if our medium changed
 *	@disk: kernel device descriptor 
 *
 *	Returns 0 if not applicable or no change; 1 if change
 *
 *	Note: this function is invoked from the block subsystem.
 **/
static int sd_media_changed(struct gendisk *disk)
{
	struct scsi_osd_disk *sdkp = scsi_osd_disk(disk);
	struct scsi_device *sdp = sdkp->device;
	int retval;

	SCSI_LOG_HLQUEUE(3, printk("sd_media_changed: disk=%s\n",
						disk->disk_name));

	if (!sdp->removable)
		return 0;

	/*
	 * If the device is offline, don't send any commands - just pretend as
	 * if the command failed.  If the device ever comes back online, we
	 * can deal with it then.  It is only because of unrecoverable errors
	 * that we would ever take a device offline in the first place.
	 */
	if (!scsi_device_online(sdp))
		goto not_present;

	/*
	 * Using TEST_UNIT_READY enables differentiation between drive with
	 * no cartridge loaded - NOT READY, drive with changed cartridge -
	 * UNIT ATTENTION, or with same cartridge - GOOD STATUS.
	 *
	 * Drives that auto spin down. eg iomega jaz 1G, will be started
	 * by sd_spinup_disk() from suo_revalidate_disk(), which happens whenever
	 * suo_revalidate_disk() is called.
	 */
	retval = -ENODEV;
	if (scsi_block_when_processing_errors(sdp))
		retval = scsi_test_unit_ready(sdp, SD_TIMEOUT, SD_MAX_RETRIES);

	/*
	 * Unable to test, unit probably not ready.   This usually
	 * means there is no disc in the drive.  Mark as changed,
	 * and we will figure it out later once the drive is
	 * available again.
	 */
	if (retval)
		 goto not_present;

	/*
	 * For removable scsi disk we have to recognise the presence
	 * of a disk in the drive. This is kept in the struct scsi_osd_disk
	 * struct and tested at open !  Daniel Roche (dan@lectra.fr)
	 */
	sdkp->media_present = 1;

	retval = sdp->changed;
	sdp->changed = 0;

	return retval;

not_present:
	set_media_not_present(sdkp);
	return 1;
}

static int sd_sync_cache(struct scsi_device *sdp)
{
	int retries, res;
	struct scsi_sense_hdr sshdr;

	if (!scsi_device_online(sdp))
		return -ENODEV;

	for (retries = 3; retries > 0; --retries) {
		unsigned char cmd[10] = { 0 };

		OSD_SET_COMMAND(OSD_FLUSH_OSD, cmd);
		cmd[2] = FLUSH_OSD_EVERYTHING;

		/*
		 * Leave the rest of the command zero to indicate
		 * flush everything.
		 */
		/* FIXME: This probably isn't right */
		res = scsi_execute_req(sdp, cmd, DMA_NONE, NULL, 0, &sshdr,
				       SD_TIMEOUT, SD_MAX_RETRIES);
		if (res == 0)
			break;
	}

	if (res) {
		printk(KERN_WARNING "FAILED\n  status = %x, message = %02x, "
				    "host = %d, driver = %02x\n  ",
				    status_byte(res), msg_byte(res),
				    host_byte(res), driver_byte(res));
		if (driver_byte(res) & DRIVER_SENSE)
			scsi_print_sense_hdr("suo", &sshdr);
	}

	return res;
}

static int sd_issue_flush(struct device *dev, sector_t *error_sector)
{
	int ret = 0;
	struct scsi_device *sdp = to_scsi_device(dev);
	struct scsi_osd_disk *sdkp = scsi_osd_disk_get_from_dev(dev);

	if (!sdkp)
               return -ENODEV;

	if (sdkp->WCE)
		ret = sd_sync_cache(sdp);
	scsi_osd_disk_put(sdkp);
	return ret;
}

static void sd_prepare_flush(request_queue_t *q, struct request *rq)
{
	/*
	memset(rq->cmd, 0, sizeof(rq->cmd));
	rq->flags |= REQ_BLOCK_PC;
	rq->timeout = SD_TIMEOUT;
	rq->cmd[0] = SYNCHRONIZE_CACHE;
	rq->cmd_len = 10;
	*/
}

static void sd_rescan(struct device *dev)
{
	struct scsi_osd_disk *sdkp = scsi_osd_disk_get_from_dev(dev);

	if (sdkp) {
		suo_revalidate_disk(sdkp->disk);
		scsi_osd_disk_put(sdkp);
	}
}


#ifdef CONFIG_COMPAT
/* 
 * This gets directly called from VFS. When the ioctl 
 * is not recognized we go back to the other translation paths. 
 */
static long sd_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct block_device *bdev = file->f_dentry->d_inode->i_bdev;
	struct gendisk *disk = bdev->bd_disk;
	struct scsi_device *sdev = scsi_osd_disk(disk)->device;

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if (!scsi_block_when_processing_errors(sdev))
		return -ENODEV;
	       
	if (sdev->host->hostt->compat_ioctl) {
		int ret;

		ret = sdev->host->hostt->compat_ioctl(sdev, cmd, (void __user *)arg);

		return ret;
	}

	/* 
	 * Let the static ioctl translation table take care of it.
	 */
	return -ENOIOCTLCMD; 
}
#endif

static struct file_operations suo_fops = {
	.owner			= THIS_MODULE,
	.open			= sd_open,
	.release		= sd_release,
	.ioctl			= sd_ioctl,
	/*  OSD Disks don't have geometry (or at least we don't care)
	.getgeo			= sd_getgeo,
	*/
#ifdef CONFIG_COMPAT
	.compat_ioctl		= sd_compat_ioctl,
#endif

	/* Char devices don't have an interface for these things
	 * FIXME: We'll have to make these available via ioctls or something
	
	.media_changed		= sd_media_changed,
	.revalidate_disk	= suo_revalidate_disk,

	*/
};

/**
 *	sd_rw_intr - bottom half handler: called when the lower level
 *	driver has completed (successfully or otherwise) a scsi command.
 *	@command: mid-level's per command structure.
 *
 *	Note: potentially run from within an ISR. Must not block.
 **/
static void sd_rw_intr(struct scsi_cmnd * command)
{
	int result = command->result;
	int this_count = command->request_bufflen;
	int good_bytes = (result == 0 ? this_count : 0);
	sector_t block_sectors = 1;
	u64 first_err_block;
	sector_t error_sector;
	struct scsi_sense_hdr sshdr;
	int sense_valid = 0;
	int sense_deferred = 0;
	int info_valid;

	if (result) {
		sense_valid = scsi_command_normalize_sense(command, &sshdr);
		if (sense_valid)
			sense_deferred = scsi_sense_is_deferred(&sshdr);
	}

#ifdef CONFIG_SCSI_LOGGING
	SCSI_LOG_HLCOMPLETE(1, printk("sd_rw_intr: %s: res=0x%x\n", 
				command->request->rq_disk->disk_name, result));
	if (sense_valid) {
		SCSI_LOG_HLCOMPLETE(1, printk("sd_rw_intr: sb[respc,sk,asc,"
				"ascq]=%x,%x,%x,%x\n", sshdr.response_code,
				sshdr.sense_key, sshdr.asc, sshdr.ascq));
	}
#endif
	/*
	   Handle MEDIUM ERRORs that indicate partial success.  Since this is a
	   relatively rare error condition, no care is taken to avoid
	   unnecessary additional work such as memcpy's that could be avoided.
	 */
	if (driver_byte(result) != 0 &&
		 sense_valid && !sense_deferred) {
		switch (sshdr.sense_key) {
			
		case MEDIUM_ERROR:
			break;

		case RECOVERED_ERROR: /* an error occurred, but it recovered */
		case NO_SENSE: /* LLDD got sense data */
			/*
			 * Inform the user, but make sure that it's not treated
			 * as a hard error.
			 */
			scsi_print_sense("sd", command);
			command->result = 0;
			memset(command->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);
			good_bytes = this_count;
			break;

		case ILLEGAL_REQUEST:
			if (command->device->use_10_for_rw &&
			    (command->cmnd[0] == READ_10 ||
			     command->cmnd[0] == WRITE_10))
				command->device->use_10_for_rw = 0;
			if (command->device->use_10_for_ms &&
			    (command->cmnd[0] == MODE_SENSE_10 ||
			     command->cmnd[0] == MODE_SELECT_10))
				command->device->use_10_for_ms = 0;
			break;

		default:
			break;
		}
	}
	/*
	 * This calls the generic completion function, now that we know
	 * how many actual sectors finished, and how many sectors we need
	 * to say have failed.
	 */
	scsi_io_completion(command, good_bytes);
}

static int media_not_present(struct scsi_osd_disk *sdkp,
			     struct scsi_sense_hdr *sshdr)
{

	if (!scsi_sense_valid(sshdr))
		return 0;
	/* not invoked for commands that could return deferred errors */
	if (sshdr->sense_key != NOT_READY &&
	    sshdr->sense_key != UNIT_ATTENTION)
		return 0;
	if (sshdr->asc != 0x3A) /* medium not present */
		return 0;

	set_media_not_present(sdkp);
	return 1;
}

/*
 * spinup disk - called only in suo_revalidate_disk()
 */
static void
sd_spinup_disk(struct scsi_osd_disk *sdkp, char *diskname)
{
	unsigned char cmd[10];
	unsigned long spintime_expire = 0;
	int retries, spintime;
	unsigned int the_result;
	struct scsi_sense_hdr sshdr;
	int sense_valid = 0;

	spintime = 0;

	/* Spin up drives, as required.  Only do this at boot time */
	/* Spinup needs to be done for module loads too. */
	do {
		retries = 0;

		do {
			cmd[0] = TEST_UNIT_READY;
			memset((void *) &cmd[1], 0, 9);

			the_result = scsi_execute_req(sdkp->device, cmd,
						      DMA_NONE, NULL, 0,
						      &sshdr, SD_TIMEOUT,
						      SD_MAX_RETRIES);

			if (the_result)
				sense_valid = scsi_sense_valid(&sshdr);
			retries++;
		} while (retries < 3 && 
			 (!scsi_status_is_good(the_result) ||
			  ((driver_byte(the_result) & DRIVER_SENSE) &&
			  sense_valid && sshdr.sense_key == UNIT_ATTENTION)));

		/*
		 * If the drive has indicated to us that it doesn't have
		 * any media in it, don't bother with any of the rest of
		 * this crap.
		 */
		if (media_not_present(sdkp, &sshdr))
			return;

		if ((driver_byte(the_result) & DRIVER_SENSE) == 0) {
			/* no sense, TUR either succeeded or failed
			 * with a status error */
			if(!spintime && !scsi_status_is_good(the_result))
				printk(KERN_NOTICE "%s: Unit Not Ready, "
				       "error = 0x%x\n", diskname, the_result);
			break;
		}
					
		/*
		 * The device does not want the automatic start to be issued.
		 */
		if (sdkp->device->no_start_on_add) {
			break;
		}

		/*
		 * If manual intervention is required, or this is an
		 * absent USB storage device, a spinup is meaningless.
		 */
		if (sense_valid &&
		    sshdr.sense_key == NOT_READY &&
		    sshdr.asc == 4 && sshdr.ascq == 3) {
			break;		/* manual intervention required */

		/*
		 * Issue command to spin up drive when not ready
		 */
		} else if (sense_valid && sshdr.sense_key == NOT_READY) {
			if (!spintime) {
				printk(KERN_NOTICE "%s: Spinning up disk...",
				       diskname);
				cmd[0] = START_STOP;
				cmd[1] = 1;	/* Return immediately */
				memset((void *) &cmd[2], 0, 8);
				cmd[4] = 1;	/* Start spin cycle */
				scsi_execute_req(sdkp->device, cmd, DMA_NONE,
						 NULL, 0, &sshdr,
						 SD_TIMEOUT, SD_MAX_RETRIES);
				spintime_expire = jiffies + 100 * HZ;
				spintime = 1;
			}
			/* Wait 1 second for next try */
			msleep(1000);
			printk(".");

		/*
		 * Wait for USB flash devices with slow firmware.
		 * Yes, this sense key/ASC combination shouldn't
		 * occur here.  It's characteristic of these devices.
		 */
		} else if (sense_valid &&
				sshdr.sense_key == UNIT_ATTENTION &&
				sshdr.asc == 0x28) {
			if (!spintime) {
				spintime_expire = jiffies + 5 * HZ;
				spintime = 1;
			}
			/* Wait 1 second for next try */
			msleep(1000);
		} else {
			/* we don't understand the sense code, so it's
			 * probably pointless to loop */
			if(!spintime) {
				printk(KERN_NOTICE "%s: Unit Not Ready, "
					"sense:\n", diskname);
				scsi_print_sense_hdr("", &sshdr);
			}
			break;
		}
				
	} while (spintime && time_before_eq(jiffies, spintime_expire));

	if (spintime) {
		if (scsi_status_is_good(the_result))
			printk("ready\n");
		else
			printk("not responding...\n");
	}
}

/* called with buffer of length 512 */
static inline int
sd_do_mode_sense(struct scsi_device *sdp, int dbd, int modepage,
		 unsigned char *buffer, int len, struct scsi_mode_data *data,
		 struct scsi_sense_hdr *sshdr)
{
	return scsi_mode_sense(sdp, dbd, modepage, buffer, len,
			       SD_TIMEOUT, SD_MAX_RETRIES, data,
			       sshdr);
}

/*
 * read write protect setting, if possible - called only in suo_revalidate_disk()
 * called with buffer of length SD_BUF_SIZE
 */
static void
sd_read_write_protect_flag(struct scsi_osd_disk *sdkp, char *diskname,
			   unsigned char *buffer)
{
	int res;
	struct scsi_device *sdp = sdkp->device;
	struct scsi_mode_data data;

	set_disk_ro(sdkp->disk, 0);
	if (sdp->skip_ms_page_3f) {
		printk(KERN_NOTICE "%s: assuming Write Enabled\n", diskname);
		return;
	}

	if (sdp->use_192_bytes_for_3f) {
		res = sd_do_mode_sense(sdp, 0, 0x3F, buffer, 192, &data, NULL);
	} else {
		/*
		 * First attempt: ask for all pages (0x3F), but only 4 bytes.
		 * We have to start carefully: some devices hang if we ask
		 * for more than is available.
		 */
		res = sd_do_mode_sense(sdp, 0, 0x3F, buffer, 4, &data, NULL);

		/*
		 * Second attempt: ask for page 0 When only page 0 is
		 * implemented, a request for page 3F may return Sense Key
		 * 5: Illegal Request, Sense Code 24: Invalid field in
		 * CDB.
		 */
		if (!scsi_status_is_good(res))
			res = sd_do_mode_sense(sdp, 0, 0, buffer, 4, &data, NULL);

		/*
		 * Third attempt: ask 255 bytes, as we did earlier.
		 */
		if (!scsi_status_is_good(res))
			res = sd_do_mode_sense(sdp, 0, 0x3F, buffer, 255,
					       &data, NULL);
	}

	if (!scsi_status_is_good(res)) {
		printk(KERN_WARNING
		       "%s: test WP failed, assume Write Enabled\n", diskname);
	} else {
		sdkp->write_prot = ((data.device_specific & 0x80) != 0);
		set_disk_ro(sdkp->disk, sdkp->write_prot);
		printk(KERN_NOTICE "%s: Write Protect is %s\n", diskname,
		       sdkp->write_prot ? "on" : "off");
		printk(KERN_DEBUG "%s: Mode Sense: %02x %02x %02x %02x\n",
		       diskname, buffer[0], buffer[1], buffer[2], buffer[3]);
	}
}

/*
 * sd_read_cache_type - called only from suo_revalidate_disk()
 * called with buffer of length SD_BUF_SIZE
 */
static void
sd_read_cache_type(struct scsi_osd_disk *sdkp, char *diskname,
		   unsigned char *buffer)
{
	int len = 0, res;
	struct scsi_device *sdp = sdkp->device;

	int dbd;
	int modepage;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;

	if (sdp->skip_ms_page_8)
		goto defaults;

	if (sdp->type == TYPE_RBC) {
		modepage = 6;
		dbd = 8;
	} else {
		modepage = 8;
		dbd = 0;
	}

	/* cautiously ask */
	res = sd_do_mode_sense(sdp, dbd, modepage, buffer, 4, &data, &sshdr);

	if (!scsi_status_is_good(res))
		goto bad_sense;

	if (!data.header_length) {
		modepage = 6;
		printk(KERN_ERR "%s: missing header in MODE_SENSE response\n",
		       diskname);
	}

	/* that went OK, now ask for the proper length */
	len = data.length;

	/*
	 * We're only interested in the first three bytes, actually.
	 * But the data cache page is defined for the first 20.
	 */
	if (len < 3)
		goto bad_sense;
	if (len > 20)
		len = 20;

	/* Take headers and block descriptors into account */
	len += data.header_length + data.block_descriptor_length;
	if (len > SD_BUF_SIZE)
		goto bad_sense;

	/* Get the data */
	res = sd_do_mode_sense(sdp, dbd, modepage, buffer, len, &data, &sshdr);

	if (scsi_status_is_good(res)) {
		int ct = 0;
		int offset = data.header_length + data.block_descriptor_length;

		if (offset >= SD_BUF_SIZE - 2) {
			printk(KERN_ERR "%s: malformed MODE SENSE response",
				diskname);
			goto defaults;
		}

		if ((buffer[offset] & 0x3f) != modepage) {
			printk(KERN_ERR "%s: got wrong page\n", diskname);
			goto defaults;
		}

		if (modepage == 8) {
			sdkp->WCE = ((buffer[offset + 2] & 0x04) != 0);
			sdkp->RCD = ((buffer[offset + 2] & 0x01) != 0);
		} else {
			sdkp->WCE = ((buffer[offset + 2] & 0x01) == 0);
			sdkp->RCD = 0;
		}

		sdkp->DPOFUA = (data.device_specific & 0x10) != 0;
		if (sdkp->DPOFUA && !sdkp->device->use_10_for_rw) {
			printk(KERN_NOTICE "SCSI device %s: uses "
			       "READ/WRITE(6), disabling FUA\n", diskname);
			sdkp->DPOFUA = 0;
		}

		ct =  sdkp->RCD + 2*sdkp->WCE;

		printk(KERN_NOTICE "SCSI device %s: drive cache: %s%s\n",
		       diskname, sd_cache_types[ct],
		       sdkp->DPOFUA ? " w/ FUA" : "");

		return;
	}

bad_sense:
	if (scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == ILLEGAL_REQUEST &&
	    sshdr.asc == 0x24 && sshdr.ascq == 0x0)
		printk(KERN_NOTICE "%s: cache data unavailable\n",
		       diskname);	/* Invalid field in CDB */
	else
		printk(KERN_ERR "%s: asking for cache data failed\n",
		       diskname);

defaults:
	printk(KERN_ERR "%s: assuming drive cache: write through\n",
	       diskname);
	sdkp->WCE = 0;
	sdkp->RCD = 0;
	sdkp->DPOFUA = 0;
}

static void
suo_read_capacity(struct scsi_osd_disk *sdkp, char *diskname,
		 unsigned char *buffer)
{
	unsigned char cmd[16];
	int the_result, retries;
	int sector_size = 0;
	int longrc = 0;
	struct scsi_sense_hdr sshdr;
	int sense_valid = 0;
	struct scsi_device *sdp = sdkp->device;

	/* FIXME: We need to send a GET_ATTRIBUTES to the root osd object.
	 * However, how the spec defines the formatting of the SCSI 
	 * command is mostly unintelligible */
	sdkp->capacity = 0;
	sdkp->device->sector_size = 512;
	return;
}


/**
 *	suo_revalidate_disk - called the first time a new disk is seen,
 *	performs disk spin up, read_capacity, etc.
 *	@disk: struct gendisk we care about
 **/
static int suo_revalidate_disk(struct genosddisk *disk)
{
	struct scsi_osd_disk *sdkp = scsi_osd_disk(disk);
	struct scsi_device *sdp = sdkp->device;
	unsigned char *buffer;
	unsigned ordered;

	SCSI_LOG_HLQUEUE(3, printk("suo_revalidate_disk: disk=%s\n", disk->disk_name));

	/*
	 * If the device is offline, don't try and read capacity or any
	 * of the other niceties.
	 */
	if (!scsi_device_online(sdp))
		goto out;

	buffer = kmalloc(SD_BUF_SIZE, GFP_KERNEL | __GFP_DMA);
	if (!buffer) {
		printk(KERN_WARNING "(suo_revalidate_disk:) Memory allocation "
		       "failure.\n");
		goto out;
	}

	/* defaults, until the device tells us otherwise */
	sdp->sector_size = 512;
	sdkp->capacity = 0;
	sdkp->media_present = 1;
	sdkp->write_prot = 0;
	sdkp->WCE = 0;
	sdkp->RCD = 0;

	sd_spinup_disk(sdkp, disk->disk_name);

	/*
	 * Without media there is no reason to ask; moreover, some devices
	 * react badly if we do.
	 */
	if (sdkp->media_present) {
		suo_read_capacity(sdkp, disk->disk_name, buffer); 
		sd_read_write_protect_flag(sdkp, disk->disk_name, buffer);
		sd_read_cache_type(sdkp, disk->disk_name, buffer);
	}
	kfree(buffer);

 out:
	return 0;
}

/* Not exported, helper to add_osd_disk(). */
void register_osd_disk(struct genosddisk *disk)
{
	struct block_device *bdev;
	char *s;
	int i;
	struct hd_struct *p;
	int err;

	strlcpy(disk->kobj.name,disk->disk_name,KOBJ_NAME_LEN);
	/* ewww... some of these buggers have / in name... */
	s = strchr(disk->kobj.name, '/');
	if (s)
		*s = '!';
	if ((err = kobject_add(&disk->kobj)))
		return;
	if (err) {
		kobject_del(&disk->kobj);
		return;
	}

exit:
	/* announce disk after possible partitions are already created */
	kobject_uevent(&disk->kobj, KOBJ_ADD);
}

int add_osd_disk(struct genosddisk *disk)
{
	int ret;
	struct cdev *chrdev;

	chrdev = cdev_alloc();
	if(!chrdev) {
		ret = -ENOMEM;
		goto out;
	}
	ret = cdev_add(chrdev, MKDEV(MAJOR(dev_id), disk->first_minor), 1);
	if(ret)
		goto out_cdev;
	register_osd_disk(disk);


	return 0;

out_cdev:
	kobject_del(&chrdev->kobj);

out:
	return ret;
}

/**
 *	suo_probe - called during driver initialization and whenever a
 *	new scsi device is attached to the system. It is called once
 *	for each scsi device (not just disks) present.
 *	@dev: pointer to device object
 *
 *	Returns 0 if successful (or not interested in this scsi device 
 *	(e.g. scanner)); 1 when there is an ret.
 *
 *	Note: this function is invoked from the scsi mid-level.
 *	This function sets up the mapping between a given 
 *	<host,channel,id,lun> (found in sdp) and new device name 
 *	(e.g. /dev/sda). More precisely it is the block device major 
 *	and minor number that is chosen here.
 *
 *	Assume sd_probe is not re-entrant (for time being)
 *	Also think about sd_probe() and suo_remove() running 
 *	coincidentally.
 **/
static int suo_probe(struct device *dev)
{
	struct scsi_device *sdp = to_scsi_device(dev);
	struct scsi_osd_disk *sdkp;
	struct genosddisk *gd;
	u32 index;
	int error;

	error = -ENODEV;
	if (sdp->type != TYPE_OSD)
		goto out;

	SCSI_LOG_HLQUEUE(3, sdev_printk(KERN_INFO, sdp,
					"suo_probe\n"));

	error = -ENOMEM;
	sdkp = kzalloc(sizeof(*sdkp), GFP_KERNEL);
	if (!sdkp)
		goto out;

	gd = alloc_osd_disk();
	if (!gd)
		goto out_free;

	/* Make sure we don't go over on devices */
	if (!idr_pre_get(&sd_index_idr, GFP_KERNEL))
		goto out_put;

	spin_lock(&sd_index_lock);
	error = idr_get_new(&sd_index_idr, NULL, &index);
	spin_unlock(&sd_index_lock);
	if (index >= SD_MAX_DISKS)
		error = -EBUSY;
	if (error)
		goto out_put;

	/* Set up sysfs and initialize our internal structure */
	class_device_initialize(&sdkp->cdev);
	sdkp->cdev.dev = &sdp->sdev_gendev;
	sdkp->cdev.class = &suo_disk_class;
	strncpy(sdkp->cdev.class_id, sdp->sdev_gendev.bus_id, BUS_ID_SIZE);

	if (class_device_add(&sdkp->cdev))
		goto out_put;

	get_device(&sdp->sdev_gendev);

	sdkp->device = sdp;
	sdkp->driver = &suo_template;
	sdkp->disk = gd;
	sdkp->index = index;
	sdkp->openers = 0;

	if (!sdp->timeout) {
		if (sdp->type != TYPE_MOD)
			sdp->timeout = SD_TIMEOUT;
		else
			sdp->timeout = SD_MOD_TIMEOUT;
	}

	gd->major = MAJOR(dev_id);
	gd->first_minor = index;
	gd->fops = &suo_fops;

	if (index < 26) {
		sprintf(gd->disk_name, SUO_DEVICE_PREFIX"%c", 'a' + index % 26);
	} else if (index < (26 + 1) * 26) {
		sprintf(gd->disk_name, SUO_DEVICE_PREFIX"%c%c",
			'a' + index / 26 - 1,'a' + index % 26);
	} else {
		const unsigned int m1 = (index / 26 - 1) / 26 - 1;
		const unsigned int m2 = (index / 26 - 1) % 26;
		const unsigned int m3 =  index % 26;
		sprintf(gd->disk_name, SUO_DEVICE_PREFIX"%c%c%c",
			'a' + m1, 'a' + m2, 'a' + m3);
	}

	gd->private_data = &sdkp->driver;
	gd->queue = sdkp->device->request_queue;

	suo_revalidate_disk(gd);

	gd->driverfs_dev = &sdp->sdev_gendev;
	gd->flags = GENHD_FL_DRIVERFS;
	if (sdp->removable)
		gd->flags |= GENHD_FL_REMOVABLE;

	dev_set_drvdata(dev, sdkp);
	add_osd_disk(gd);

	sdev_printk(KERN_NOTICE, sdp, "Attached scsi %sdisk %s\n",
		    sdp->removable ? "removable " : "", gd->disk_name);

	return 0;

 out_put:
	put_osd_disk(gd);
 out_free:
	kfree(sdkp);
 out:
	return error;
}

/**
 *	suo_remove - called whenever a scsi disk (previously recognized by
 *	suo_probe) is detached from the system. It is called (potentially
 *	multiple times) during sd module unload.
 *	@sdp: pointer to mid level scsi device object
 *
 *	Note: this function is invoked from the scsi mid-level.
 *	This function potentially frees up a device name (e.g. /dev/sdc)
 *	that could be re-used by a subsequent suo_probe().
 *	This function is not called when the built-in sd driver is "exit-ed".
 **/
static int suo_remove(struct device *dev)
{
	struct scsi_osd_disk *sdkp = dev_get_drvdata(dev);

	class_device_del(&sdkp->cdev);
	del_gendisk(sdkp->disk);
	sd_shutdown(dev);

	mutex_lock(&sd_ref_mutex);
	dev_set_drvdata(dev, NULL);
	class_device_put(&sdkp->cdev);
	mutex_unlock(&sd_ref_mutex);

	return 0;
}


/**
 *	scsi_osd_disk_release - Called to free the scsi_osd_disk structure
 *	@cdev: pointer to embedded class device
 *
 *	sd_ref_mutex must be held entering this routine.  Because it is
 *	called on last put, you should always use the scsi_osd_disk_get()
 *	scsi_osd_disk_put() helpers which manipulate the semaphore directly
 *	and never do a direct class_device_put().
 **/
static void scsi_osd_disk_release(struct class_device *cdev)
{
	struct scsi_osd_disk *sdkp = to_osd_disk(cdev);
	struct genosddisk *disk = sdkp->disk;
	
	spin_lock(&sd_index_lock);
	idr_remove(&sd_index_idr, sdkp->index);
	spin_unlock(&sd_index_lock);

	disk->private_data = NULL;
	put_osd_disk(disk);
	put_device(&sdkp->device->sdev_gendev);

	kfree(sdkp);
}

/*
 * Send a FLUSH OSD instruction down to the device through
 * the normal SCSI command structure.  Wait for the command to
 * complete.
 */
static void sd_shutdown(struct device *dev)
{
	struct scsi_device *sdp = to_scsi_device(dev);
	struct scsi_osd_disk *sdkp = scsi_osd_disk_get_from_dev(dev);

	if (!sdkp)
		return;         /* this can happen */

	if (sdkp->WCE) {
		printk(KERN_NOTICE "Synchronizing SCSI cache for disk %s: \n",
				sdkp->disk->disk_name);
		sd_sync_cache(sdp);
	}
	scsi_osd_disk_put(sdkp);
}

/**
 *	init_suo - entry point for this driver (both when built in or when
 *	a module).
 *
 *	Note: this function registers this driver with the scsi mid-level.
 **/
static int __init init_suo(void)
{
	int ret;
	int has_registered_chrdev = 0;
	so_sysfs_class = NULL;

	SCSI_LOG_HLQUEUE(3, printk("init_suo: suo driver entry point\n")); 
	if(major > 0) {
		dev_id = (major ? MKDEV(major, 0) : 0);
		ret = register_chrdev_region(dev_id, SUO_MAX_OSD_ENTRIES, "suo");
	}
	else
		ret = alloc_chrdev_region(&dev_id, 0, SUO_MAX_OSD_ENTRIES, "suo");

	if (ret != 0) {
		printk(KERN_DEBUG "Can't %s the chardev region! Error code is 0x%x (%d)\n", 
				  (major > 0 ? "register" : "dynamically allocate"), ret);
		return -ENODEV;
	}
	has_registered_chrdev = -1;

	so_sysfs_class = class_create(THIS_MODULE, "scsi_osd");
	if (IS_ERR(so_sysfs_class)) {
		ret = PTR_ERR(so_sysfs_class);
		goto bail;
	}

	if(ret = scsi_register_driver(&suo_template.gendrv)) 
		goto bail;
	
	return 0;


bail:
	/* TODO: Unregister SCSI if anything gets added after the
	 * driver registration */
	if(so_sysfs_class)	class_destroy(so_sysfs_class);
	if(has_registered_chrdev) {
		unregister_chrdev_region(dev_id, SUO_MAX_OSD_ENTRIES);
	}
	return ret;
}

/**
 *	exit_suo - exit point for this driver (when it is a module).
 *
 *	Note: this function unregisters this driver from the scsi mid-level.
 **/
static void __exit exit_suo(void)
{
	int i;

	SCSI_LOG_HLQUEUE(3, printk("exit_suo: exiting sd driver\n"));

	scsi_unregister_driver(&suo_template.gendrv);
	if(so_sysfs_class)	class_destroy(so_sysfs_class);
	unregister_chrdev_region(dev_id, SUO_MAX_OSD_ENTRIES);
}

module_init(init_suo);
module_exit(exit_suo);
