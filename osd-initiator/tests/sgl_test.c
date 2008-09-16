/*
 * Test device scatter/gather IO.
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


static void basic_test_sgl(int fd, uint64_t pid, uint64_t oid)
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
	memset(xbuf, '0', 100);
	read_osd(fd, pid, oid, xbuf, 100, 0);


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

	/* ---------------------------- */
	/* Test Append command with SGL */
	/* ---------------------------- */

	/* 50 bytes of data plus 5 (offset,len) pairs plus the length value */
	size = 50 + (2*sizeof(uint64_t) * 5) + sizeof(uint64_t);

	free(dbuf);
	dbuf = malloc(size);
	memset(dbuf, 'A', size);

	set_htonll(dbuf, 5);
	hdr_offset = sizeof(uint64_t);

	offset = 0;
	length = 10;
	for (i=0; i<5; i++ ) {
		osd_debug("Offset= %llu  Length= %llu", llu(offset), llu(length));
		set_htonll((uint8_t *)dbuf + hdr_offset, offset);
		offset += length*2;
		hdr_offset += sizeof(uint64_t);
		set_htonll((uint8_t *)dbuf + hdr_offset, length);
		hdr_offset += sizeof(uint64_t);
	}

	ret = append_sgl_osd(fd, pid, oid, dbuf, size);
	assert(ret == 0);

	/* ------------------------------------------------------- */
	/* Now check what we just wrote with existing read command */
	/* ------------------------------------------------------- */
	free(xbuf);
	xbuf = malloc(200);
	memset(xbuf, 'Z', 200);
	read_osd(fd, pid, oid, xbuf, 190, 0);  /*10 less b/c way we scatter*/

	for (i=0; i<190; i++) {
		char ch;
		memcpy(&ch, (char *)xbuf+i, 1);
		if(ch == '\0')
			printf("~");
		else
			printf("%c", ch);
	}
	printf("\n");

	/* ------------------------------------------------ */
	/*Test the create and write functionality - non SGL */
	/* ------------------------------------------------ */
	free(xbuf);
	xbuf = malloc(100);
	memset(xbuf, 'Y', 100);

	ret = create_and_write_osd(fd, pid, oid+1, xbuf, 100, 0);
	assert(ret == 0);

	free(xbuf);
	xbuf = malloc(100);
	memset(xbuf, '\0', 100);

	read_osd(fd, pid, oid+1, xbuf, 100, 0);
	printf("%s\n", (char *) xbuf);

	/* ------------------------------------------------ */
	/*Test the create and write functionality ----  SGL */
	/* ------------------------------------------------ */

	/* 50 bytes of data plus 5 (offset,len) pairs plus the length value */
	size = 50 + (2*sizeof(uint64_t) * 5) + sizeof(uint64_t);
	free(dbuf);
	dbuf = malloc(size);
	memset(dbuf, 'D', size);

	set_htonll(dbuf, 5);
	hdr_offset = sizeof(uint64_t);
	offset = 0;
	length=10;

	for(i=0; i<5; i++){
		osd_debug("Offset= %llu  Length= %llu", llu(offset), llu(length));
		set_htonll((uint8_t *)dbuf + hdr_offset, offset);
		offset += length*2;
		hdr_offset += sizeof(uint64_t);
		set_htonll((uint8_t *)dbuf + hdr_offset, length);
		hdr_offset += sizeof(uint64_t);
	}

	ret = create_and_write_sgl_osd(fd, pid, oid+2, dbuf, size, 0);
	assert(ret == 0);

	free(xbuf);
	xbuf = malloc(100);
	memset(xbuf, 'Z', 100);
	read_osd(fd, pid, oid+2, xbuf, 90, 0);  /*10 less b/c way we scatter*/

	for (i=0; i<100; i++) {
		char ch;
		memcpy(&ch, (char *)xbuf+i, 1);
		if(ch == '\0')
			printf("~");
		else
			printf("%c", ch);
	}
	printf("\n");


}


static void basic_test_vec(int fd, uint64_t pid, uint64_t oid)
{

	/* format of the data buff is:
	|stride| |length| [DATA]
	all values are 8 bytes */

	void *dbuf;
	void *xbuf;
	void *buf;
	uint64_t length = 10, stride = 20;
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

	/* 50 bytes of data plus stride and length value */
	size = 50 + (2*sizeof(uint64_t));

	dbuf = malloc(size);
	memset(dbuf, 'D', size);

	set_htonll((uint8_t *)dbuf, stride);
	hdr_offset = sizeof(uint64_t);
	set_htonll((uint8_t *)dbuf + hdr_offset, length);

	ret = write_vec_osd(fd, pid, oid, dbuf, size, 0);
	assert(ret == 0);

	/* ------------------------------------------------------- */
	/* Now check what we just wrote with existing read command */
	/* ------------------------------------------------------- */
	memset(xbuf, '0', 100);
	read_osd(fd, pid, oid, xbuf, 100, 0);

	/* ---------------------------------------------- */
	/*  Now try to gather the Ds we wrote previously  */
	/* ---------------------------------------------- */
	free(dbuf);
	dbuf = malloc(50);
	memset(dbuf, 'Z', 50);

	size = 2*sizeof(uint64_t);
	buf = malloc(size);
	set_htonll((uint8_t *)buf, stride);
	hdr_offset = sizeof(uint64_t);
	set_htonll((uint8_t *)buf + hdr_offset, length);

	ret = read_vec_osd(fd, pid, oid, buf, size, dbuf, 50, 0);

	/* ---------------------------- */
	/* Test Append command with SGL */
	/* ---------------------------- */

	/* 50 bytes of data plus 5 (offset,len) pairs plus the length value */
	size = 50 + (2*sizeof(uint64_t));

	dbuf = malloc(size);
	memset(dbuf, 'A', size);

	set_htonll((uint8_t *)dbuf, stride);
	hdr_offset = sizeof(uint64_t);
	set_htonll((uint8_t *)dbuf + hdr_offset, length);

	ret = append_vec_osd(fd, pid, oid, dbuf, size);
	assert(ret == 0);

	/* ------------------------------------------------------- */
	/* Now check what we just wrote with existing read command */
	/* ------------------------------------------------------- */
	free(xbuf);
	xbuf = malloc(200);
	memset(xbuf, 'Z', 200);
	read_osd(fd, pid, oid, xbuf, 190, 0);  /*10 less b/c way we scatter*/

	for (i=0; i<190; i++) {
		char ch;
		memcpy(&ch, (char *)xbuf+i, 1);
		if(ch == '\0')
			printf("~");
		else
			printf("%c", ch);
	}
	printf("\n");

	/* ------------------------------------------------ */
	/*Test the create and write functionality - non VEC */
	/* ------------------------------------------------ */
	free(xbuf);
	xbuf = malloc(100);
	memset(xbuf, 'Y', 100);

	ret = create_and_write_osd(fd, pid, oid+1, xbuf, 100, 0);
	assert(ret == 0);

	free(xbuf);
	xbuf = malloc(100);
	memset(xbuf, '\0', 100);

	read_osd(fd, pid, oid+1, xbuf, 100, 0);
	printf("%s\n", (char *) xbuf);

	/* ------------------------------------------------ */
	/*Test the create and write functionality ----  VEC */
	/* ------------------------------------------------ */

	/* 50 bytes of data plus stride and length value */
	size = 50 + (2*sizeof(uint64_t));
	free(dbuf);
	dbuf = malloc(size);
	memset(dbuf, 'D', size);

	set_htonll((uint8_t *)dbuf, stride);
	hdr_offset = sizeof(uint64_t);
	set_htonll((uint8_t *)dbuf + hdr_offset, length);

	ret = create_and_write_vec_osd(fd, pid, oid+2, dbuf, size, 0);
	assert(ret == 0);

	free(xbuf);
	xbuf = malloc(100);
	memset(xbuf, 'Z', 100);
	read_osd(fd, pid, oid+2, xbuf, 90, 0);  /*10 less b/c way we scatter*/

	for (i=0; i<100; i++) {
		char ch;
		memcpy(&ch, (char *)xbuf+i, 1);
		if(ch == '\0')
			printf("~");
		else
			printf("%c", ch);
	}
	printf("\n");


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

	/*run tests*/
	oid = obj_create_any(fd, PARTITION_PID_LB);
	basic_test_sgl(fd, PARTITION_PID_LB, oid);
	obj_remove(fd, PARTITION_PID_LB, oid);
	obj_remove(fd, PARTITION_PID_LB, oid+1);
	obj_remove(fd, PARTITION_PID_LB, oid+2);


	printf("\n\n\nRunning VECTOR tests now\n\n\n");


	oid = obj_create_any(fd, PARTITION_PID_LB);
	basic_test_vec(fd, PARTITION_PID_LB, oid);
	obj_remove(fd, PARTITION_PID_LB, oid);

	close(fd);

	return 0;
}
