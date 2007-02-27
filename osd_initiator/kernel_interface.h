#ifndef _KERNEL_INTERFACE_H
#define _KERNEL_INTERFACE_H

#define OSD_CDB_SIZE 200
#define OSD_MAX_SENSE 252

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
	void *indata;   		/* [o] results, returned from target */
	size_t inlen_alloc;		/* [i] alloc size for results */
	size_t inlen;      	 	/* [o] actual size returned */
	uint8_t status;     		/* [o] scsi status */
	uint8_t sense[OSD_MAX_SENSE];	/* [o] sense errors */
	int sense_len;      		/* [o] number of bytes in sense */
};

void osd_show_sense(uint8_t *sense, int len);
int osd_sgio_submit_command(int fd, struct osd_command *command);
int osd_sgio_wait_response(int fd, struct osd_command **command);
int osd_sgio_submit_and_wait(int fd, struct osd_command *command);
int osd_sgio_submit_and_wait_python(int fd, uint8_t *cdb, int cdb_len,
                                    void *outdata, size_t outlen,
				    size_t inlen_alloc);

#endif  /* _KERNEL_INTERFACE_H */
