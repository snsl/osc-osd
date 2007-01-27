#ifndef _SUO_IOCTL_H
#define _SUO_IOCTL_H 

/* FIXME: I can't think of a more clever way to fix this problem */
typedef uint64_t ptr_t;

/* FIXME: How do we get the value of SCSI_SENSE_BUFFERSIZE without including
 * the kernel headers? */
/* XXX: osd max could be 252.  Make sure kernel captures all that. */
#define OSD_SENSE_BUFFERSIZE 252

/*
 * Passed from userspace to kernel to initiate a SCSI request.  Arranged
 * so it packs nicely on 8-byte boundaries.
 */
struct suo_req {
	uint32_t data_direction;  /* enum dma_data_direction */
	uint32_t cdb_len;
	ptr_t cdb_buf;
	uint32_t in_data_len;
	uint32_t out_data_len;
	ptr_t in_data_buf;
	ptr_t out_data_buf;
	uint64_t key;
};

struct suo_response {
	uint64_t key;
	int error;
	int sense_buffer_len;
	unsigned char sense_buffer[OSD_SENSE_BUFFERSIZE];
};

#endif /* _SUO_IOCTL_H */
