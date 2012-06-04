/*
 * Use the BSG interface in the kernel to submit SCSI commands and retrieve
 * responses.
 *
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "osd-util/osd-util.h"
#include <linux/bsg.h>
#include <scsi/sg.h>
#include "command.h"
#include "device.h"

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
#ifdef KERNEL_SUPPORTS_BSG_IOVEC
		sg.dout_xfer_len = command->outlen;
		sg.dout_xferp = (uint64_t) (uintptr_t) command->outdata;
		sg.dout_iovec_count = command->iov_outlen;
#else
		// The kernel doesn't support BSG iovecs mainly because
		// of a problem going from 32-bit user iovecs to a 64-bit kernel
		// So, just copy the iovecs into a new buffer and use that
		sg_iovec_t *iov = (sg_iovec_t *)(uintptr_t)command->outdata;
		if (command->iov_outlen == 0) {
			sg.dout_xfer_len = command->outlen;
			sg.dout_xferp = (uint64_t) (uintptr_t) command->outdata;
		} else if (command->iov_outlen == 1) {
			sg.dout_xfer_len = iov->iov_len;
			sg.dout_xferp = (uint64_t) (uintptr_t) iov->iov_base;
		} else {
			int i;
			uint8_t *buff = Malloc(command->outlen);
			sg.dout_xferp = (uint64_t) (uintptr_t) buff;
			for (i=0; i<command->iov_outlen; i++) {
				memcpy(buff, iov[i].iov_base, iov[i].iov_len);
				buff += iov[i].iov_len;
			}
			sg.dout_xfer_len = command->outlen;
		}
		sg.dout_iovec_count = 0;
#endif
	}
	if (command->inlen_alloc) {
#ifdef KERNEL_SUPPORTS_BSG_IOVEC
		sg.din_xfer_len = command->inlen_alloc;
		sg.din_xferp = (uint64_t) (uintptr_t) command->indata;
		sg.din_iovec_count = command->iov_inlen;
#else
		if (command->iov_inlen == 0) {
			sg.din_xfer_len = command->inlen_alloc;
			sg.din_xferp = (uint64_t) (uintptr_t) command->indata;
			sg.din_iovec_count = command->iov_inlen;
		} else if (command->iov_inlen == 1) {
			sg_iovec_t *iov = (sg_iovec_t *)command->indata;
			sg.din_xfer_len = iov->iov_len;
			sg.din_xferp = (uint64_t) (uintptr_t) iov->iov_base;
		} else {
			sg.din_xfer_len = command->inlen_alloc;
			sg.din_xferp = (uint64_t) (uintptr_t) (uint8_t*) Malloc(command->inlen_alloc);
		}
		sg.din_iovec_count = 0;
#endif
	}

	/*
	 * Allow 30 sec for entire command.  Some can be
	 * slow, especially with debugging messages on.
	 */
	sg.timeout = 30000;
	sg.usr_ptr = (uint64_t) (uintptr_t) command;
	ret = write(fd, &sg, sizeof(sg));
#ifndef KERNEL_SUPPORTS_BSG_IOVEC
	if (command->outlen && command->iov_outlen > 1) {
		free((void *) (uintptr_t) sg.dout_xferp);
	}
#endif
	if (ret < 0) {
		osd_error_errno("%s: write", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		osd_error("%s: short write, %d not %zu", __func__, ret,
			  sizeof(sg));
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

	command = (void *)(uintptr_t) sg.usr_ptr;
	if (command->inlen_alloc)
		command->inlen = command->inlen_alloc - sg.din_resid;
	command->status = sg.device_status;
	command->sense_len = sg.response_len;

#ifndef KERNEL_SUPPORTS_BSG_IOVEC
	// copy from buffer to iovecs
	if (command->inlen_alloc && command->iov_inlen > 1) {
		sg_iovec_t *iov = (sg_iovec_t *) command->indata;
		uint8_t *buff = (uint8_t *) (uintptr_t) sg.din_xferp;
		int i;
		for (i=0; i<command->iov_inlen; i++) {
			memcpy(iov[i].iov_base, buff, iov[i].iov_len);
			buff += iov[i].iov_len;
		}
		free((void *) (uintptr_t) sg.din_xferp);
	}
#endif

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

	ret = osd_submit_command(fd, command);
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	ret = osd_wait_this_response(fd, command);
	if (ret) {
		osd_error("%s: wait_response failed", __func__);
		return ret;
	}
	return 0;
}
