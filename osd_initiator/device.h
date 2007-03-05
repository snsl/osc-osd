#ifndef _DEVICE_H
#define _DEVICE_H

#define OSD_CDB_SIZE 200
#define OSD_MAX_SENSE 252

/*
 * This is copied from a kernel header to avoid including it.
 */
struct bsg_iovec {
	uint64_t iov_base;
	uint32_t iov_len;
	uint32_t __pad1;
};

/*
 * All information needed to submit a command to the kernel module.
 *   [i] = provided by caller to submit_osd()
 *   [o] = return from library to caller
 */
struct osd_command {
	uint8_t cdb[OSD_CDB_SIZE];	/* [i] maximum length CDB */
	int cdb_len;        		/* [i] actual len of valid bytes */
	const void *outdata; 		/* [i] data, goes out to target */
	size_t outlen;      		/* [i] length */
	int iov_outlen;                 /* [i] if non-zero, data are iovecs */
	void *indata;   		/* [o] results, returned from target */
	size_t inlen_alloc;		/* [i] alloc size for results */
	size_t inlen;      	 	/* [o] actual size returned */
	int iov_inlen;                  /* [i] */
	uint8_t status;     		/* [o] scsi status */
	uint8_t sense[OSD_MAX_SENSE];	/* [o] sense errors */
	int sense_len;      		/* [o] number of bytes in sense */
};

int osd_submit_command(int fd, struct osd_command *command);
int osd_wait_response(int fd, struct osd_command **command);
int osd_submit_and_wait(int fd, struct osd_command *command);

#endif  /* _DEVICE_H */
