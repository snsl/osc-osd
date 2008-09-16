/*
 * Test the use of the existing SG_IO interface to transport OSD commands.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "sync.h"
#include "sense.h"

static int check_response(int ret, struct osd_command *command,
			  uint8_t *buf __unused)
{
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		return ret;
	}
	if ((command->status != 0) && (command->sense_len != 0)) {
		osd_error("status: %u sense len: %u inlen: %zu",
			  command->status, command->sense_len, command->inlen);
		osd_error("%s ", osd_sense_as_string(command->sense,
			  command->sense_len));

	} else if (command->inlen > 0) {
		osd_debug("Successfully performed task. BUF: <<%s>>", buf);
	} else {
		osd_debug("Successfully performed task");
	}

	return 0;
}

int inquiry(int fd)
{
	const int INQUIRY_RSP_LEN = 80;
	int ret;
	uint8_t inquiry_rsp[INQUIRY_RSP_LEN];
	struct osd_command command;

	osd_debug("****** INQUIRY ******");

	osd_debug("....creating command");
	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));
	osd_command_set_inquiry(&command, 0, sizeof(inquiry_rsp));

	osd_debug("....building command");
	command.indata = inquiry_rsp;
	command.inlen_alloc = sizeof(inquiry_rsp);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, inquiry_rsp);

#ifndef NDEBUG
	osd_hexdump(inquiry_rsp, command.inlen);
	printf("\n");
#endif

	return 0;
}

int query(int fd, uint64_t pid, uint64_t cid, const uint8_t *query)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_debug("****** QUERY ******");
	osd_debug("PID: %llu CID: %llu QUERY: %s", llu(pid), llu(cid), query);

	if (!query) {
		osd_error("%s: no query sent", __func__);
		return 1;
	}

	osd_debug("....creating command");
	osd_command_set_query(&command, pid, cid,
			      strlen((const char *)query), sizeof(buf));

	osd_debug("....building command");
	command.outdata = query;
	command.outlen = strlen((const char *)query) + 1;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, buf);

	return 0;
}

int create_osd(int fd, uint64_t pid, uint64_t requested_oid,
	       uint16_t num_user_objects)
{
	int ret;
	struct osd_command command;

	osd_debug("****** CREATE OBJECT ******");
	osd_debug("Creating %u objects", num_user_objects);
	osd_debug("PID: %llu OID: %llu OBJ: %u",
		llu(pid), llu(requested_oid), num_user_objects);

	osd_debug("....creating command");
	osd_command_set_create(&command, pid, requested_oid, num_user_objects);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int create_partition(int fd, uint64_t requested_pid)
{
	int ret;
	struct osd_command command;

	osd_debug("****** CREATE PARTITION ******");
	osd_debug("PID: %llu", llu(requested_pid));

	osd_debug("....creating command");
	osd_command_set_create_partition(&command, requested_pid);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int create_collection(int fd, uint64_t pid, uint64_t requested_cid)
{
	int ret;
	struct osd_command command;

	osd_debug("****** CREATE COLLECTION ******");
	osd_debug("PID: %llu CID: %llu", llu(pid), llu(requested_cid));

	osd_debug("....creating command");
	osd_command_set_create_collection(&command, pid, requested_cid);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int remove_osd(int fd, uint64_t pid, uint64_t requested_oid)
{
	int ret;
	struct osd_command command;

	osd_debug("****** REMOVE OBJECT ******");
	osd_debug("PID: %llu OID: %llu", llu(pid), llu(requested_oid));

	osd_debug("....creating command");
	osd_command_set_remove(&command, pid, requested_oid);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int remove_partition(int fd, uint64_t pid)
{
	int ret;
	struct osd_command command;

	osd_debug("****** REMOVE PARTITION ******");
	osd_debug("PID: %llu", llu(pid));

	osd_debug("....creating command");
	osd_command_set_remove_partition(&command, pid);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int remove_collection(int fd, uint64_t pid, uint64_t cid, int force)
{
	int ret;
	struct osd_command command;

	osd_debug("****** REMOVE COLLECTION ******");
	osd_debug("PID: %llu CID: %llu", llu(pid), llu(cid));

	osd_debug("....creating command");
	osd_command_set_remove_collection(&command, pid, cid, force);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int remove_member_objects(int fd, uint64_t pid, uint64_t cid)
{
	int ret;
	struct osd_command command;

	osd_debug("****** REMOVE MEMBER OBJECTS ******");
	osd_debug("PID: %llu CID: %llu", llu(pid), llu(cid));

	osd_debug("....creating command");
	osd_command_set_remove_member_objects(&command, pid, cid);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int create_and_write_osd(int fd, uint64_t pid, uint64_t requested_oid,
			 const uint8_t *buf, uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** CREATE / WRITE ******");
	osd_debug("PID: %llu OID: %llu BUF: %s", llu(pid), llu(requested_oid),
		  buf);

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	osd_debug("....creating command");
	osd_command_set_create_and_write(&command, pid, requested_oid, len,
					 offset);

	osd_debug("....building command");
	command.outdata = buf;
	command.outlen = len;

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int create_and_write_sgl_osd(int fd, uint64_t pid, uint64_t requested_oid,
			 const uint8_t *buf, uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** CREATE / WRITE SGL ******");
	osd_debug("PID: %llu OID: %llu BUF: %s", llu(pid), llu(requested_oid),
		  buf);

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	osd_command_set_create_and_write(&command, pid, requested_oid, len,
					 offset);

	osd_command_set_ddt(&command, DDT_SGL);

	command.outdata = buf;
	command.outlen = len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int create_and_write_vec_osd(int fd, uint64_t pid, uint64_t requested_oid,
			 const uint8_t *buf, uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** CREATE / WRITE VEC ******");
	osd_debug("PID: %llu OID: %llu BUF: %s", llu(pid), llu(requested_oid),
		  buf);

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	osd_command_set_create_and_write(&command, pid, requested_oid, len,
					 offset);

	osd_command_set_ddt(&command, DDT_VEC);

	command.outdata = buf;
	command.outlen = len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int write_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	      uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** WRITE ******");
	osd_debug("PID: %llu OID: %llu BUF: %s", llu(pid), llu(oid), buf);

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	osd_command_set_write(&command, pid, oid, len, offset);

	command.outdata = buf;
	command.outlen = len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int write_sgl_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	      uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** WRITE SGL ******");
	osd_debug("PID: %llu OID: %llu LEN: %llu", llu(pid), llu(oid), llu(len));

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	/*setting the command is now a two stage process and not doing it
	will default to contiguous buffer model*/
	osd_command_set_write(&command, pid, oid, len, offset);

	osd_command_set_ddt(&command, DDT_SGL);

	command.outdata = buf;
	command.outlen = len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int write_vec_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	      uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** WRITE VEC ******");
	osd_debug("PID: %llu OID: %llu LEN: %llu", llu(pid), llu(oid), llu(len));

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	/*setting the command is now a two stage process and not doing it
	will default to contiguous buffer model*/
	osd_command_set_write(&command, pid, oid, len, offset);

	osd_command_set_ddt(&command, DDT_VEC);

	command.outdata = buf;
	command.outlen = len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int append_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	       uint64_t len)
{
	int ret;
	struct osd_command command;

	osd_debug("****** APPEND ******");
	osd_debug("PID: %llu OID: %llu BUF: %s", llu(pid), llu(oid), buf);

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	osd_debug("....creating command");
	osd_command_set_append(&command, pid, oid, len);

	osd_debug("....building command");
	command.outdata = buf;
	command.outlen = len;

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int append_sgl_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	       uint64_t len)
{
	int ret;
	struct osd_command command;

	osd_debug("****** APPEND ******");
	osd_debug("PID: %llu OID: %llu BUF: %s", llu(pid), llu(oid), buf);

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	osd_command_set_append(&command, pid, oid, len);

	osd_command_set_ddt(&command, DDT_SGL);


	command.outdata = buf;
	command.outlen = len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int append_vec_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	       uint64_t len)
{
	int ret;
	struct osd_command command;

	osd_debug("****** APPEND ******");
	osd_debug("PID: %llu OID: %llu BUF: %s", llu(pid), llu(oid), buf);

	if (!buf) {
		osd_error("%s: no data sent", __func__);
		return 1;
	}

	osd_command_set_append(&command, pid, oid, len);

	osd_command_set_ddt(&command, DDT_VEC);


	command.outdata = buf;
	command.outlen = len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int read_osd(int fd, uint64_t pid, uint64_t oid, uint8_t *buf, uint64_t len,
	     uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** READ ******");
	osd_debug("PID: %llu OID: %llu", llu(pid), llu(oid));

	osd_debug("....creating command");
	osd_command_set_read(&command, pid, oid, len, offset);

	osd_debug("....building command");
	command.indata = buf;
	command.inlen_alloc = len;

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, buf);

	return 0;
}

int read_sgl_osd(int fd, uint64_t pid, uint64_t oid, uint8_t *ddt_buf,
			uint64_t ddt_len, uint8_t *buf, uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** READ ******");
	osd_debug("PID: %llu OID: %llu", llu(pid), llu(oid));
	osd_debug("ddt len: %llu len: %llu", llu(ddt_len), llu(len));

	osd_command_set_read(&command, pid, oid, len, offset);

	osd_command_set_ddt(&command, DDT_SGL);

	command.indata = buf;
	command.inlen_alloc = len;

	command.outdata = ddt_buf;
	command.outlen = ddt_len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, buf);

	return 0;
}

int read_vec_osd(int fd, uint64_t pid, uint64_t oid, uint8_t *ddt_buf,
			uint64_t ddt_len, uint8_t *buf, uint64_t len, uint64_t offset)
{
	int ret;
	struct osd_command command;

	osd_debug("****** READ ******");
	osd_debug("PID: %llu OID: %llu", llu(pid), llu(oid));
	osd_debug("ddt len: %llu len: %llu", llu(ddt_len), llu(len));

	osd_command_set_read(&command, pid, oid, len, offset);

	osd_command_set_ddt(&command, DDT_VEC);

	command.indata = buf;
	command.inlen_alloc = len;

	command.outdata = ddt_buf;
	command.outlen = ddt_len;

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, buf);

	return 0;
}

int format_osd(int fd, int capacity)
{
	int ret;
	struct osd_command command;

	osd_debug("****** FORMAT OSD ******");
	osd_debug("Capacity: %i", capacity);

	osd_debug("....creating command");
	osd_command_set_format_osd(&command, capacity);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int flush_osd(int fd, int flush_scope)
{
	int ret;
	struct osd_command command;

	osd_debug("****** FLUSH OSD ******");

	osd_debug("....creating command");
	osd_command_set_flush_osd(&command, flush_scope);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int flush_partition(int fd, uint64_t pid, int flush_scope)
{
	int ret;
	struct osd_command command;

	osd_debug("****** FLUSH PARTITION ******");
	osd_debug("PID: %llu", llu(pid));

	osd_debug("....creating command");
	osd_command_set_flush_partition(&command, pid, flush_scope);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int flush_collection(int fd, uint64_t pid, uint64_t cid, int flush_scope)
{
	int ret;
	struct osd_command command;

	osd_debug("****** FLUSH COLLECTION ******");
	osd_debug("PID: %llu CID: %llu", llu(pid), llu(cid));

	osd_debug("....creating command");
	osd_command_set_flush_collection(&command, pid, cid, flush_scope);

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

int flush_object(int fd, uint64_t pid, uint64_t oid, int flush_scope)
{
	int ret;
	struct osd_command command;

	osd_debug("****** FLUSH OBJECT ******");
	osd_debug("PID: %llu OID: %llu", llu(pid), llu(oid));

	osd_command_set_flush(&command, pid, oid, flush_scope);

	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	ret = osd_wait_this_response(fd, &command);
	check_response(ret, &command, NULL);

	return 0;
}

/* Attributes */

/* Unimplemented */
int get_attributes(int fd, uint64_t pid, uint64_t oid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_debug("****** GET ATTRIBUTES ******");
	osd_debug("PID: %llu OID: %llu", llu(pid), llu(oid));

	osd_command_set_get_attributes(&command, pid, oid);
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);
	memset(buf, 0, sizeof(buf));
	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, &command, buf);
	return 0;
}

/* Unimplemented */
int set_attributes(int fd, uint64_t pid, uint64_t oid,
		   const struct attribute_list *attrs)
{
	int ret;
	struct osd_command command;

	osd_debug("****** SET ATTRIBUTES ******");
	osd_debug("PID: %llu OID: %llu", llu(pid), llu(oid));

	if (!attrs) {
		osd_error("%s: no attributes sent", __func__);
		return 1;
	}

	osd_command_set_set_attributes(&command, pid, oid);
	command.outdata = attrs;
	command.outlen = sizeof(*attrs);
	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, &command, NULL);
	return 0;
}

/* Unimplemented */
int set_member_attributes(int fd, uint64_t pid, uint64_t cid,
			  const struct attribute_list *attrs)
{
	int ret;
	struct osd_command command;

	osd_debug("****** SET MEMBER ATTRIBUTES ******");
	osd_debug("PID: %llu CID: %llu", llu(pid), llu(cid));
	if (!attrs) {
		osd_error("%s: no attributes sent", __func__);
		return 1;
	}

	osd_command_set_set_member_attributes(&command, pid, cid);
	command.outdata = attrs;
	command.outlen = sizeof(*attrs);
	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, &command, NULL);
	return 0;
}

/* Unimplemented */
int get_member_attributes(int fd, uint64_t pid, uint64_t cid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_debug("****** GET MEMBER ATTRIBUTES ******");
	osd_debug("PID: %llu CID: %llu", llu(pid), llu(cid));

	osd_command_set_get_member_attributes(&command, pid, cid);
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);
	memset(buf, 0, sizeof(buf));
	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, &command, buf);
	return 0;
}


/*
 * List
 * sec 6.14 in spec.
*/
int list(int fd, uint64_t pid, uint32_t list_id, uint64_t initial_oid,
	 uint64_t alloc_len, int list_attr)
{
 	/*
	 * buf_len specifies how many bytes the buffer for the returned data
	 * will be.
	 * It is 24 bytes larger than alloc_len for header information in the
	 * returned data.
	*/
	int ret;
	uint64_t buf_len = alloc_len + 24;
	uint8_t buf[buf_len];
	memset(buf, 0, buf_len);
	struct osd_command command;

	osd_debug("****** LIST ******");
        /*
	 * If pid is 0, we would like to list all the PIDs that exist, where
	 * initial_oid represents the PID to start at. If pid is nonzero,
	 * we wish to list all the OIDs, starting with initial_oid, in the
	 * partition given by pid.
	*/
	if (list_id == 0) {
		if (pid == 0) {
			osd_debug("INITIAL_PID: %llu", llu(initial_oid));
		} else {
			osd_debug("PID: %llu INITIAL_OID: %llu", llu(pid),
				  llu(initial_oid));
		}
	}
	else {
		osd_debug("LIST_ID: %u CONTINUATION_ID: %llu", list_id,
			  llu(initial_oid));
	}

	osd_debug("....creating command");
	osd_command_set_list(&command, pid, list_id, buf_len, initial_oid,
			     list_attr);

	osd_debug("....building command");
	command.indata = buf;
	command.inlen_alloc = buf_len;

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	osd_command_list_resolve(&command);

	return 0;
}

/*
 * List Collection
 * sec 6.15 in spec.
*/
int list_collection(int fd, uint64_t pid, uint64_t cid, uint32_t list_id,
		    uint64_t initial_oid, uint64_t alloc_len, int list_attr)
{
 	/*
	 * buf_len specifies how many bytes the buffer for the returned data
	 * will be.
	 * It is 24 bytes larger than alloc_len for header information in the
	 * returned data.
	*/
	int ret;
	uint64_t buf_len = alloc_len + 24;
	uint8_t buf[buf_len];
	memset(buf, 0, buf_len);
	struct osd_command command;

	osd_debug("****** LIST COLLECTION ******");
        /*
	 * If cid is 0, we would like to list all the CIDs in the
	 * given partition, and initial_oid is the lowest CID to start at.
	 * If cid is nonzero, we would like to list all the OIDs in the
	 * collection given by cid, and initial_oid is the lowest OID to start
	 * at.
	*/
	if (list_id == 0) {
		if (cid == 0) {
			osd_debug("PID: %llu INITIAL_CID: %llu", llu(pid),
				  llu(initial_oid));
		} else {
			osd_debug("PID: %llu CID: %llu INITIAL_OID: %llu",
				  llu(pid), llu(cid), llu(initial_oid));
		}
	}
	else {
		osd_debug("LIST_ID: %u CONTINUATION_ID: %llu", list_id,
			  llu(initial_oid));
	}

	osd_debug("....creating command");
	osd_command_set_list_collection(&command, pid, cid, list_id, buf_len,
				        initial_oid, list_attr);

	osd_debug("....building command");
	command.indata = buf;
	command.inlen_alloc = buf_len;

	osd_debug("....submitting command");
	ret = osd_submit_command(fd, &command);
	check_response(ret, &command, NULL);

	osd_debug("....retrieving response");
	ret = osd_wait_this_response(fd, &command);
	osd_command_list_collection_resolve(&command);

	return 0;
}
