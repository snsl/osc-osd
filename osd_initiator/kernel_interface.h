#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "kernel/suo_ioctl.h"

#define FIRST_USER_PARTITION (0x10000LLU)
#define FIRST_USER_OBJECT (0x10000LLU)

#define OSD_CDB_SIZE 200
#define OSD_MAX_SENSE 252

/* These are only typedef'd because Pyrex won't pick them up
 * correctly otherwise */

enum data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

/*
 * All information needed to submit a command to the kernel module.
 *   [i] = provided by caller to submit_osd()
 *   [o] = return from library to caller
 */
struct osd_command {
	uint8_t cdb[OSD_CDB_SIZE];   /* [i] maximum length CDB */
	int cdb_len;        /* [i] actual len of valid bytes */
	const void *outdata;/* [i] data for command, goes out to target */
	size_t outlen;      /* [i] length */
	void *indata;       /* [o] results from command, returned from target */
	size_t inlen_alloc; /* [i] allocated size for command results */
	size_t inlen;       /* [o] actual size returned */
	uint8_t status;     /* [o] scsi status */
	uint8_t sense[OSD_MAX_SENSE]; /* [o] sense errors */
	int sense_len;      /* [o] number of bytes in sense */
/* maybe..	uint64_t tag; */      /* [x] ignored, for convenient tracking */
};

/*
 * Public functions
 */

int dev_osd_open(const char *dev);
void dev_osd_close(int fd);
int dev_osd_wait_response(int fd, struct suo_response *devresp);
int dev_osd_bidir(int fd, const uint8_t *cdb, int cdb_len, const void *outbuf,
		   size_t outlen, void *inbuf, size_t inlen);
void dev_show_sense(uint8_t *sense, int len);

/*
 * XXX: new functions, please give better names and implement.
 */
int osd_submit_command(int fd, struct osd_command *command);
int osd_retrieve_result(int fd, struct osd_command **command);

/*
 * Experimental stuff, ignore for now.
 */
int osd_sgio_submit_command(int fd, struct osd_command *command);
int osd_sgio_wait_response(int fd, struct osd_command **command);
int osd_sgio_submit_and_wait(int fd, struct osd_command *command);
int osd_sgio_submit_and_wait_python(int fd, uint8_t *cdb, int cdb_len,
                                    void *outdata, size_t outlen,
				    size_t inlen_alloc);

#endif
