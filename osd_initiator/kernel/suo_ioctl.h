#ifndef _SUO_IOCTL_H
#define _SUO_IOCTL_H 

/*
 * Passed from userspace to kernel to initiate a SCSI request.  Arranged
 * so it packs nicely on 8-byte boundaries.
 */
struct suo_req {
	__u32 data_direction;  /* enum dma_data_direction */
	__u32 cdb_len;
	__u64 cdb_buf;
	__u32 in_data_len;
	__u32 out_data_len;
	__u64 in_data_buf;
	__u64 out_data_buf;
};

#endif /* _SUO_IOCTL_H */
