/*
 * Talk to the kernel module.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#ifdef USE_BSG
typedef  int32_t __s32;
typedef uint32_t __u32;
typedef uint64_t __u64;
#include <linux/bsg.h>
#else
#include <scsi/sg.h>
#endif

#include "util/util.h"
#include "kernel_interface.h"

/* description of the sense key values */
static const char *const snstext[] = {
	"No Sense",	    /* 0: There is no sense information */
	"Recovered Error",  /* 1: The last command completed successfully
				  but used error correction */
	"Not Ready",	    /* 2: The addressed target is not ready */
	"Medium Error",	    /* 3: Data error detected on the medium */
	"Hardware Error",   /* 4: Controller or device failure */
	"Illegal Request",  /* 5: Error in request */
	"Unit Attention",   /* 6: Removable medium was changed, or
				  the target has been reset, or ... */
	"Data Protect",	    /* 7: Access to the data is blocked */
	"Blank Check",	    /* 8: Reached unexpected written or unwritten
				  region of the medium */
	"Vendor Specific(9)",
	"Copy Aborted",	    /* A: COPY or COMPARE was aborted */
	"Aborted Command",  /* B: The target aborted the command */
	"Equal",	    /* C: A SEARCH DATA command found data equal */
	"Volume Overflow",  /* D: Medium full with still data to be written */
	"Miscompare",	    /* E: Source data and data on the medium
				  do not agree */
};

/*
 * Possibly debugging support.  Parse and print out interesting bits of the
 * sense (i.e. error) message returned from the target.  Maybe suck in all
 * the strings from linux/drivers/scsi/constants.c someday.  In a separate
 * file.
 */
void osd_show_sense(uint8_t *sense, int len)
{
	uint8_t code, key, asc, ascq, additional_len;
	uint8_t *info;
	uint64_t pid, oid;
	const char *keystr = "(unknown)";

	/* osd_hexdump(sense, len); */
	if (len < 8) {
		printf("%s: sense length too short\n", __func__);
		return;
	}
	code = sense[0];
	if ((code & 0x72) != 0x72) {
		printf("%s: code 0x%02x not expected 0x72 or 0x73\n",
		       __func__, code);
		return;
	}
	key = sense[1];
	asc = sense[2];
	ascq = sense[3];
	if (key < ARRAY_SIZE(snstext))
		keystr = snstext[key];
	printf("%s: %s key 0x%02x %s asc 0x%02x ascq 0x%02x\n", __func__,
	       (code & 1) ? "deferred" : "current", key, keystr, asc, ascq);
	additional_len = sense[7];
	if (additional_len < 32) {
		printf("%s: additional len %d not enough for OSD info\n",
		       __func__, additional_len);
		return;
	}
	info = sense + 8;
	if (info[0] != 0x6) {
		printf("%s: unexpected data descriptor type 0x%02x\n",
		       __func__, info[0]);
		return;
	}
	if (info[1] != 0x1e) {
		printf("%s: invalid info length 0x%02x\n", __func__, info[1]);
		return;
	}
	/* ignore not_init and completed funcs */
	pid = ntohll(&info[16]);
	oid = ntohll(&info[16]);
	printf("%s: offending pid 0x%016lx oid 0x%016lx\n", __func__, pid, oid);
}

int osd_sgio_submit_command(int fd, struct osd_command *command)
{
	int ret;
#ifdef USE_BSG
	struct sg_io_v4 sg;
#else
	struct sg_io_hdr sg;
	void *buf = NULL;
	unsigned int len = 0;
	int dir = SG_DXFER_NONE;
#endif

	memset(&sg, 0, sizeof(sg));
#ifdef USE_BSG
	sg.guard = 'Q';
	sg.request_len = command->cdb_len;
	sg.request = (uint64_t) (uintptr_t) command->cdb;
	sg.max_response_len = sizeof(command->sense);
	sg.response = (uint64_t) (uintptr_t) command->sense;

	if (command->outlen) {
		sg.dout_xfer_len = command->outlen;
		sg.dout_xferp = (uint64_t) (uintptr_t) command->outdata;
	}
	if (command->inlen_alloc) {
		sg.din_xfer_len = command->inlen_alloc;
		sg.din_xferp = (uint64_t) (uintptr_t) command->indata;
	}
	sg.timeout = 3000;
	sg.usr_ptr = (uint64_t) (uintptr_t) command;
#else
	if (command->outlen) {
		if (command->inlen_alloc) {
			osd_error("%s: bidirectional not supported", __func__);
			return -EINVAL;
		} else {
			buf = (void *)(uintptr_t)command->outdata;
			len = command->outlen;
			dir = SG_DXFER_TO_DEV;
		}
	} else if (command->inlen_alloc) {
		buf = command->indata;
		len = command->inlen_alloc;
		dir = SG_DXFER_FROM_DEV;
	}
	sg.interface_id = 'S';
	sg.dxfer_direction = dir;
	sg.cmd_len = command->cdb_len;
	sg.mx_sb_len = sizeof(command->sense);
	sg.dxfer_len = len;
	sg.dxferp = buf;
	sg.cmdp = command->cdb;
	sg.sbp = command->sense;
	sg.timeout = 3000;
	sg.usr_ptr = command;
#endif
	ret = write(fd, &sg, sizeof(sg));
	if (ret < 0) {
		osd_error_errno("%s: write", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		osd_error("%s: short write, %d not %zu", __func__, ret, sizeof(sg));
		return -EIO;
	}
	return 0;
}

int osd_sgio_wait_response(int fd, struct osd_command **out_command)
{
#ifdef USE_BSG
	struct sg_io_v4 sg;
#else
	struct sg_io_hdr sg;
#endif
	struct osd_command *command;
	int ret;

	ret = read(fd, &sg, sizeof(sg));
	if (ret < 0) {
		osd_error_errno("%s: read", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		osd_error("%s: short read, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

#ifdef USE_BSG
	command = (void *) sg.usr_ptr;
	if (command->inlen_alloc)
		command->inlen = command->inlen_alloc - sg.din_resid;
	command->status = sg.device_status;
	command->sense_len = sg.response_len;
#else
/*	
	printf("%s: status %u sense len %u resid %d\n", __func__, sg.status,
	       sg.sb_len_wr, sg.resid);
*/	
	command = sg.usr_ptr;

	if (command->inlen_alloc)
		command->inlen = command->inlen_alloc - sg.resid;

	command->status = sg.status;
	command->sense_len = sg.sb_len_wr;
#endif

	*out_command = command;

	return 0;
}

int osd_sgio_submit_and_wait(int fd, struct osd_command *command)
{
	int ret;
	struct osd_command *cmp;

	ret = osd_sgio_submit_command(fd, command);
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	ret = osd_sgio_wait_response(fd, &cmp);
	if (ret) {
		osd_error("%s: wait_response failed", __func__);
		return ret;
	}
	if (cmp != command) {
		osd_error("%s: wait_response returned %p, expecting %p", __func__,
		      cmp, command);
		return 1;
	}
	return 0;
}

/*
 * Experimental, pending we figure out how to get python to talk
 * to these other functions.
 */
int osd_sgio_submit_and_wait_python(int fd, uint8_t *cdb, int cdb_len,
                                    void *outdata, size_t outlen,
				    size_t inlen_alloc)
{
	int ret;
	struct osd_command command;

	memcpy(command.cdb, cdb, cdb_len);
	command.cdb_len = cdb_len;
	command.outdata = outdata;
	command.outlen = outlen;
	command.inlen_alloc = inlen_alloc;
	ret = osd_sgio_submit_and_wait(fd, &command);
	return ret;
}

