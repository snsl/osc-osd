#ifndef _SUO_IOCTL_H
#define _SUO_IOCTL_H 

#include <linux/autoconf.h>

/* FIXME: I can't think of a more clever way to fix this problem */
typedef uint64_t ptr_t ;

/*
 * Passed from userspace to kernel to initiate a SCSI request.  Arranged
 * so it packs nicely on 8-byte boundaries.
 */
struct suo_req {
	uint32_t data_direction;  /* enum dma_data_direction */
	uint32_t cdb_len;
	uint64_t cdb_buf;
	uint32_t in_data_len;
	uint32_t out_data_len;
	uint64_t in_data_buf;
	uint64_t out_data_buf;
	uint64_t key;
};

struct suo_response {
	uint64_t key;

};

#endif /* _SUO_IOCTL_H */
