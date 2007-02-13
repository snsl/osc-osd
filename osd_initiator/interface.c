/*
 * Talk to the kernel module.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <scsi/sg.h>

#include <stdint.h>
#include <sys/types.h>

#include "util/util.h"

#include "interface.h"

/*Functions for user codes to manipulate the character device*/
int dev_osd_open(const char *dev)
{
	int interface_fd = open(dev, O_RDWR);
	return interface_fd;
}

void dev_osd_close(int fd)
{
	close(fd);
}

/* blocking */
int dev_osd_wait_response(int fd, struct suo_response *devresp)
{
	int ret;

	ret = read(fd, devresp, sizeof(struct suo_response));
	if (ret < 0)
		error_errno("%s: read response", __func__);
	if (ret != sizeof(struct suo_response))
		error("%s: got %d bytes, expecting response %zu bytes",
		      __func__, ret, sizeof(struct suo_response));
	
	return 0;
}

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
void dev_show_sense(uint8_t *sense, int len)
{
	uint8_t code, key, asc, ascq, additional_len;
	uint8_t *info;
	uint64_t pid, oid;
	const char *keystr = "(unknown)";

	/* hexdump(sense, len); */
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
	struct sg_io_hdr sg;
	int dir = SG_DXFER_NONE;
	void *buf = NULL;
	unsigned int len = 0;
	int ret;

	if (command->outlen) {
		if (command->inlen_alloc) {
			error("%s: bidirectional not supported", __func__);
			return 1;
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
	memset(&sg, 0, sizeof(sg));
	sg.interface_id = 'S';
	sg.dxfer_direction = dir;
	sg.cmd_len = command->cdb_len;
	sg.mx_sb_len = command->sense_len;
	sg.dxfer_len = len;
	sg.dxferp = buf;
	sg.cmdp = command->cdb;
	sg.sbp = command->sense;
	sg.timeout = 3000;
	sg.usr_ptr = command;
	ret = write(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: write", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short write, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}
	return 0;
}

int osd_sgio_wait_response(int fd, struct osd_command **out_command)
{
	struct sg_io_hdr sg;
	struct osd_command *command;
	int ret;

	ret = read(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: read", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short read, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	printf("%s: status %u sense len %u resid %d\n", __func__, sg.status,
	       sg.sb_len_wr, sg.resid);
	command = sg.usr_ptr;

	if (command->inlen_alloc)
		command->inlen = command->inlen_alloc - sg.resid;
	command->status = sg.status;
	command->sense_len = sg.sb_len_wr;

	*out_command = command;

	return 0;
}

int osd_sgio_submit_and_wait(int fd, struct osd_command *command)
{
	int ret;
	struct osd_command *cmp;

	ret = osd_sgio_submit_command(fd, command);
	if (ret) {
		error("%s: submit failed", __func__);
		return ret;
	}

	ret = osd_sgio_wait_response(fd, &cmp);
	if (ret) {
		error("%s: wait_response failed", __func__);
		return ret;
	}
	if (cmp != command) {
		error("%s: wait_response returned %p, expecting %p", __func__,
		      cmp, command);
		return 1;
	}
	return 0;
}

int osd_submit_command(int fd, struct osd_command *command)
{
	struct suo_req req;
	int ret;
	enum data_direction dir = DMA_NONE;

	if (command->outlen) {
		if (command->inlen)
			dir = DMA_BIDIRECTIONAL;
		else
			dir = DMA_TO_DEVICE;
	} else if (command->inlen) {
		dir = DMA_FROM_DEVICE;
	}
	req.data_direction = dir;
	req.cdb_len = command->cdb_len;
	req.cdb_buf = (uint64_t) (uintptr_t) command->cdb;
	req.out_data_len = (uint32_t) command->outlen;
	req.out_data_buf = (uint64_t) (uintptr_t) command->outdata;
	req.in_data_len = (uint32_t) command->inlen;
	req.in_data_buf = (uint64_t) (uintptr_t) command->indata;
	info("%s: cdb[0] %02x len %d inbuf %p len %zu outbuf %p len %zu",
	     __func__, command->cdb[0], command->cdb_len, command->indata, 
	    command->inlen, command->outdata, command->outlen);
	ret = write(fd, &req, sizeof(req));
	if (ret < 0)
		error_errno("%s: write suo request", __func__);
	return ret;
}

