/*
 * Talk to the kernel module.
 */
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "util/util.h"
#include "command.h"
#include "device.h"

/*
 * Include kernel header directly (bad).  Need to define some kernel
 * data types for that to work.
 */
typedef  int32_t __s32;
typedef uint32_t __u32;
typedef uint64_t __u64;
#include <linux/bsg.h>

int osd_submit_command(int fd, struct osd_command *command)
{
	int ret;
	struct sg_io_v4 sg;

	memset(&sg, 0, sizeof(sg));
	sg.guard = 'Q';
	sg.request_len = command->cdb_len;
	sg.request = (uint64_t) (uintptr_t) command->cdb;
	sg.max_response_len = sizeof(command->sense);
	sg.response = (uint64_t) (uintptr_t) command->sense;

	if (command->outlen) {
		sg.dout_xfer_len = command->outlen;
		sg.dout_xferp = (uint64_t) (uintptr_t) command->outdata;
		sg.dout_iovec_count = command->iov_outlen;
	}
	if (command->inlen_alloc) {
		sg.din_xfer_len = command->inlen_alloc;
		sg.din_xferp = (uint64_t) (uintptr_t) command->indata;
		sg.din_iovec_count = command->iov_inlen;
	}
	sg.timeout = 3000;
	sg.usr_ptr = (uint64_t) (uintptr_t) command;
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

int osd_wait_response(int fd, struct osd_command **out_command)
{
	struct sg_io_v4 sg;
	struct osd_command *command;
	int ret;

	ret = read(fd, &sg, sizeof(sg));
	if (ret < 0) {
		osd_error_errno("%s: read", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		osd_error("%s: short read, %d not %zu", __func__, ret,
		          sizeof(sg));
		return -EPIPE;
	}

	command = (void *) sg.usr_ptr;
	if (command->inlen_alloc)
		command->inlen = command->inlen_alloc - sg.din_resid;
	command->status = sg.device_status;
	command->sense_len = sg.response_len;

	*out_command = command;

	return 0;
}

/*
 * osd_wait_response, plus verify that the command retrieved was the
 * one we expected, or error.
 */
int osd_wait_this_response(int fd, struct osd_command *command)
{
	int ret;
	struct osd_command *cmp;

	ret = osd_wait_response(fd, &cmp);
	if (ret == 0) {
		if (cmp != command) {
			osd_error("%s: wrong command returned", __func__);
			ret = -EIO;
		}
	}
	return ret;
}

int osd_submit_and_wait(int fd, struct osd_command *command)
{
	int ret;
	struct osd_command *cmp;

	ret = osd_submit_command(fd, command);
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	ret = osd_wait_this_response(fd, cmp);
	if (ret) {
		osd_error("%s: wait_response failed", __func__);
		return ret;
	}
	if (cmp != command) {
		osd_error("%s: wait_response returned %p, expecting %p",
		          __func__, cmp, command);
		return 1;
	}
	return 0;
}

