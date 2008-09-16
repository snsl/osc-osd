#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>

#include "util/osd-util.h"
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
	ret = osd_command_attr_resolve(&command);
	if (ret) {
		osd_error_xerrno(ret, "%s: attr_resolve failed", __func__);
		exit(1);
	}
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


static void test_sgl(int fd, uint64_t pid, uint64_t oid)
{

	/* format of the data buff is:
	|#offset,len pairs| |offset| |length|....[DATA]
	all values are 8 bytes */

	void *dbuf;
	void *xbuf;
	void *buf;
	uint64_t length = 10;
	int size;
	int ret;
	int hdr_offset = 0;
	int offset = 0;
	int i;

	/* -------------------------- */
	/*  write a buffer of 100 Xs  */
	/* -------------------------- */
	xbuf = malloc(100);
	memset(xbuf, 'X', 100);
	write_osd(fd, pid, oid, xbuf, 100, 0);

	/* --------------------------------------------------------------- */
	/*  use scatter and fill in with Ds every other chunk of 10 chars  */
	/* --------------------------------------------------------------- */

	/* 50 bytes of data plus 5 (offset,len) pairs plus the length value */
	size = 50 + (2*sizeof(uint64_t) * 5) + sizeof(uint64_t);

	dbuf = malloc(size);
	memset(dbuf, 'D', size);

	set_htonll(dbuf, 5);
	hdr_offset += sizeof(uint64_t);

	for(i=0; i<5; i++){
		osd_debug("Offset= %llu  Length= %llu", llu(offset), llu(length));
		set_htonll((uint8_t *)dbuf + hdr_offset, offset);
		offset += length*2;
		hdr_offset += sizeof(uint64_t);
		set_htonll((uint8_t *)dbuf + hdr_offset, length);
		hdr_offset += sizeof(uint64_t);
	}

	ret = write_sgl_osd(fd, pid, oid, dbuf, size, 0);
	assert(ret == 0);

	/* ------------------------------------------------------- */
	/* Now check what we just wrote with existing read command */
	/* ------------------------------------------------------- */
	memset(xbuf, 'Z', 100);
	read_osd(fd, pid, oid, xbuf, 100, 0);
	printf("%s\n", (char *)xbuf);

	/* ---------------------------------------------- */
	/*  Now try to gather the Ds we wrote previously  */
	/* ---------------------------------------------- */

	size = (2*sizeof(uint64_t) * 5) + sizeof(uint64_t);
	buf = malloc(size);
	memset(dbuf, 'Z', size);

	set_htonll(buf, 5);
	hdr_offset = 0;
	hdr_offset += sizeof(uint64_t);
	offset = 0;
	for (i=0; i<5; i++) {
		osd_debug("Offset= %llu  Length= %llu", llu(offset), llu(length));
		set_htonll((uint8_t *)buf + hdr_offset, offset);
		offset += length*2;
		hdr_offset += sizeof(uint64_t);
		set_htonll((uint8_t *)buf + hdr_offset, length);
		hdr_offset += sizeof(uint64_t);
	}
	dbuf = malloc(50);  /* return buffer */
	ret = read_sgl_osd(fd, pid, oid, buf, size, dbuf, 50, 0);
	printf("%s\n", (char *)buf);






}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;
	uint64_t oid;

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
	osd_debug("drive %s name %s", drives[i].chardev, drives[i].targetname);
	fd = open(drives[i].chardev, O_RDWR);
	if (fd < 0) {
		osd_error_errno("%s: open %s", __func__, drives[i].chardev);
		return 1;
	}
	osd_free_drive_list(drives, num_drives);

	inquiry(fd);

	format_osd(fd, 1<<30);
	create_partition(fd, PARTITION_PID_LB);

	oid = obj_create_any(fd, PARTITION_PID_LB);

	/*run tests*/
	test_sgl(fd, PARTITION_PID_LB, oid);

	obj_remove(fd, PARTITION_PID_LB, oid);

	close(fd);

	return 0;
}
