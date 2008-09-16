/*
 * Test big and odd iovecs for read and write.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

static uint64_t obj_create_any(int fd, uint64_t pid)
{
	struct osd_command command;
	struct attribute_list attr = {
		.type = ATTR_GET,
		.page = CUR_CMD_ATTR_PG,
		.number = CCAP_OID,
		.len = 8,
	};
	int ret;
	uint64_t oid;

	osd_command_set_create(&command, pid, 0, 0);
	osd_command_attr_build(&command, &attr, 1);
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit_and_wait failed", __func__);
		exit(1);
	}
	osd_command_attr_resolve(&command);
	oid = get_ntohll(command.attr[0].val);
	osd_command_attr_free(&command);
	return oid;
}

static void obj_remove(int fd, uint64_t pid, uint64_t oid)
{
	struct osd_command command;
	int ret;

	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit_and_wait failed", __func__);
		exit(1);
	}
}

/*
 * Numvec limit imposed by bio when allocating bvecs.  256 is max, but
 * if something strattles a page boundary, that costs an extra vec.  So
 * here 256 is likely to fail.
 */
static void iovec_write_test(int fd, uint64_t pid)
{
	struct osd_command command;
	struct bsg_iovec *vec;
	int ret, i, numvec;
	uint64_t oid;
	uint8_t *buf;
	const uint32_t buflen = (300 << 10) + 12;

	buf = Malloc(buflen);
	for (numvec=1; numvec<=128; numvec *= 2) {
		memset(buf, 0x5a, buflen);
		oid = obj_create_any(fd, pid);
		osd_info("%s: write numvec %d buflen %d", __func__, numvec,
			 buflen);
		if (numvec == 0)
			vec = (void *) buf;
		else {
			uint32_t onesize = buflen / numvec;
			uint32_t pos = 0;
			vec = Malloc(numvec * sizeof(*vec));
			for (i=0; i<numvec; i++) {
				vec[i].iov_base = (uintptr_t) buf + pos;
				vec[i].iov_len = onesize;
				pos += onesize;
			}
			if (pos < buflen)
				vec[numvec-1].iov_len += buflen - pos;
		}
		osd_command_set_write(&command, pid, oid, buflen, 0);
		command.outlen = buflen;
		command.outdata = vec;
		command.iov_outlen = numvec;
		ret = osd_submit_and_wait(fd, &command);
		if (ret) {
			osd_error("%s: write submit_and_wait failed", __func__);
			return;
		}
		if (numvec > 0)
			free(vec);

		/* read it back, non-iov */
		osd_info("%s: read  numvec %d buflen %d", __func__, numvec,
			 buflen);
		memset(buf, 0, buflen);
		osd_command_set_read(&command, pid, oid, buflen, 0);
		command.inlen_alloc = buflen;
		command.indata = buf;
		ret = osd_submit_and_wait(fd, &command);
		if (ret)
			osd_error("%s: read submit_and_wait failed", __func__);

		for (i=0; i<(int)buflen; i++)
			if (buf[i] != 0x5a) {
				osd_error("%s: wrong byte %02x at pos %d",
					  __func__, buf[i], i);
				return;
			}

		obj_remove(fd, pid, oid);
	}
	free(buf);
}

static void iovec_read_test(int fd, uint64_t pid)
{
	struct osd_command command;
	struct bsg_iovec *vec;
	int ret, i, numvec;
	uint64_t oid;
	uint8_t *buf;
	const uint32_t buflen = (300 << 10) + 12;

	buf = Malloc(buflen);
	for (numvec=1; numvec<=128; numvec *= 2) {
		oid = obj_create_any(fd, pid);

		/* write it, non-iov */
		osd_info("%s: write numvec %d buflen %d", __func__, numvec,
			 buflen);
		memset(buf, 0x5a, buflen);
		osd_command_set_write(&command, pid, oid, buflen, 0);
		command.outlen = buflen;
		command.outdata = buf;
		ret = osd_submit_and_wait(fd, &command);
		if (ret)
			osd_error("%s: write submit_and_wait failed", __func__);

		osd_info("%s: read  numvec %d buflen %d", __func__, numvec,
			 buflen);
		memset(buf, 0, buflen);
		if (numvec == 0)
			vec = (void *) buf;
		else {
			uint32_t onesize = buflen / numvec;
			uint32_t pos = 0;
			vec = Malloc(numvec * sizeof(*vec));
			for (i=0; i<numvec; i++) {
				vec[i].iov_base = (uintptr_t) buf + pos;
				vec[i].iov_len = onesize;
				pos += onesize;
			}
			if (pos < buflen)
				vec[numvec-1].iov_len += buflen - pos;
		}
		osd_command_set_read(&command, pid, oid, buflen, 0);
		command.inlen_alloc = buflen;
		command.indata = vec;
		command.iov_inlen = numvec;
		ret = osd_submit_and_wait(fd, &command);
		if (ret) {
			osd_error("%s: read submit_and_wait failed", __func__);
			return;
		}
		if (numvec > 0)
			free(vec);

		for (i=0; i<(int)buflen; i++)
			if (buf[i] != 0x5a) {
				osd_error("%s: wrong byte %02x at pos %d",
					  __func__, buf[i], i);
				return;
			}

		obj_remove(fd, pid, oid);
	}
	free(buf);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;

	osd_set_progname(argc, argv); 

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		osd_error("%s: get drive error", __func__);
		return 1;
	}
	if (num_drives == 0) {
		osd_error("%s: no drives", __func__);
		return 1;
	}
	
	i = 0;
	printf("%s: drive %s name %s\n", osd_get_progname(), drives[i].chardev,
	       drives[i].targetname);
	fd = open(drives[i].chardev, O_RDWR);
	if (fd < 0) {
		osd_error_errno("%s: open %s", __func__, drives[i].chardev);
		return 1;
	}
	osd_free_drive_list(drives, num_drives);

	inquiry(fd);

	format_osd(fd, 1<<30); 
	create_partition(fd, PARTITION_PID_LB);

	iovec_write_test(fd, PARTITION_PID_LB);
	iovec_read_test(fd, PARTITION_PID_LB);

	close(fd);
	return 0;
}

