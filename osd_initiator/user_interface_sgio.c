/*
 * Test the use of the existing SG_IO interface to transport OSD commands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include "util/util.h"
#include "kernel_interface.h"
#include "cdb_manip.h"
#include "user_interface_sgio.h"
#include "diskinfo.h"

static int check_response(int ret, struct osd_command command, uint8_t buf[])
{
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}	
	if (command.status != 0)
		osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
			command.status, command.sense_len, command.inlen);
	else if (command.inlen > 0)
		osd_info("Successfully performed task. BUF: <<%s>>", buf);
	else 
		osd_info("Successfully performed task");

	printf("\n");
	return 0;
}

int inquiry(int fd)
{
	const int INQUIRY_RSP_LEN = 80;
	int ret;
	uint8_t inquiry_rsp[INQUIRY_RSP_LEN];
	struct osd_command command;

	osd_info("****** INQUIRY ******");

	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));
	osd_command_set_inquiry(&command, sizeof(inquiry_rsp));
	command.indata = inquiry_rsp;
	command.inlen_alloc = sizeof(inquiry_rsp);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

	osd_hexdump(inquiry_rsp, command.inlen);

	printf("\n");
	return 0;
}

int query(int fd, uint64_t pid, uint64_t cid, const char *query)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;
	
	osd_info("****** QUERY ******");
	osd_info("PID: %u CID: %u QUERY: %s", (uint)pid, (uint)cid, query);

	if (query) {
		osd_command_set_query(&command, pid, cid, strlen(query), sizeof(buf));
		command.outdata = query;
		command.outlen = strlen(query) + 1;
		command.indata = buf;
		command.inlen_alloc = sizeof(buf);

		ret = osd_submit_and_wait(fd, &command);
		check_response(ret, command, buf);
	}
	else 
		osd_error("%s: no query sent", __func__); 

	return 0;
}
 
int create_osd(int fd, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects)
{
	int ret;
	struct osd_command command;

	osd_info("****** CREATE OBJECT ******");
	osd_info("Creating %u objects", (uint)num_user_objects);

	osd_info("PID: %u OID: %u OBJ: %uu", 
		(uint)pid, (uint)requested_oid, num_user_objects);

	/* Create user objects one at a time */ 
	osd_command_set_create(&command, pid, requested_oid, num_user_objects);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);
	return 0;
}

int create_partition(int fd, uint64_t requested_pid)
{
	int ret;
	struct osd_command command;

	osd_info("****** CREATE PARTITION ******");
	osd_info("PID: %u", (uint)requested_pid);


	osd_command_set_create_partition(&command, requested_pid);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

	return 0;
}

int create_collection(int fd, uint64_t pid, uint64_t requested_cid)
{
	int ret;
	struct osd_command command;

	osd_info("****** CREATE COLLECTION ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)requested_cid);

	osd_command_set_create_collection(&command, pid, requested_cid);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);
	return 0;
}

int remove_osd(int fd, uint64_t pid, uint64_t requested_oid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE OBJECT ******");
	osd_info("PID: %u OID: %u", (uint)pid, (uint)requested_oid);

        osd_command_set_remove(&command, pid, requested_oid);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int remove_partition(int fd, uint64_t pid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE PARTITION ******");
	osd_info("PID: %u", (uint)pid);

        osd_command_set_remove_partition(&command, pid);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int remove_collection(int fd, uint64_t pid, uint64_t cid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE COLLECTION ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

        osd_command_set_remove_collection(&command, pid, cid);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int remove_member_objects(int fd, uint64_t pid, uint64_t cid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE MEMBER OBJECTS ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

        osd_command_set_remove_member_objects(&command, pid, cid);

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int create_osd_and_write(int fd, uint64_t pid, uint64_t requested_oid, const char *buf, uint64_t offset)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** CREATE / WRITE ******");
	osd_info("PID: %u OID: %u BUF: %s", (uint)pid, (uint)requested_oid, buf);

	if (buf) {
		osd_command_set_create_and_write(&command, pid, requested_oid, strlen(buf) + 1, offset);
		command.outdata = buf;
		command.outlen = strlen(buf) + 1;
	
		ret = osd_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else
		osd_error("%s: no data sent", __func__);

	return 0;
}

int write_osd(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** WRITE ******");
	osd_info("PID: %u OID: %u BUF: %s", (uint)pid, (uint)oid, buf);

	if (buf) {
		osd_command_set_write(&command, pid, oid, strlen(buf) + 1, offset);
		command.outdata = buf;
		command.outlen = strlen(buf) + 1;

		ret = osd_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else 
		osd_error("%s: no data sent", __func__); 

	return 0;
}

int read_osd(int fd, uint64_t pid, uint64_t oid, uint64_t offset)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** READ ******");
	osd_info("PID: %u OID: %u EMPTY BUF: <<%s>>", (uint)pid, (uint)oid, buf);

	osd_command_set_read(&command, pid, oid, sizeof(buf), offset);
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

int format_osd(int fd, int capacity)
{
        int ret;
        struct osd_command command;

        osd_info("****** FORMAT OSD ******"); 
	osd_info("Capacity: %i", capacity);

        osd_command_set_format_osd(&command, capacity);

	ret = osd_submit_and_wait(fd, &command);	
	check_response(ret, command, NULL);
	
        return 0;
}

int flush_osd(int fd, int flush_scope)
{
        int ret;
        struct osd_command command;

        osd_info("****** FLUSH OSD ******"); 

        osd_command_set_flush_osd(&command, flush_scope);   
	
	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0; 
}

int flush_partition(int fd, uint64_t pid, int flush_scope)
{
        int ret;
        struct osd_command command;

        osd_info("****** FLUSH PARTITION ******"); 
	osd_info("PID: %u", (uint)pid);

        osd_command_set_flush_partition(&command, pid, flush_scope);   
	
	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0; 
}

int flush_collection(int fd, uint64_t pid, uint64_t cid, int flush_scope)
{
        int ret;
        struct osd_command command;

        osd_info("****** FLUSH COLLECTION ******"); 
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

        osd_command_set_flush_collection(&command, pid, cid, flush_scope);   
	
	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0; 
}

int get_attributes(int fd, uint64_t pid, uint64_t oid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** GET ATTRIBUTES ******");
	osd_info("PID: %u OID: %u EMPTY BUF: <<%s>>", (uint)pid, (uint)oid, buf);

	osd_command_set_get_attributes(&command, pid, oid);
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

int set_attributes(int fd, uint64_t pid, uint64_t oid, const struct attribute_id *attrs)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** SET ATTRIBUTES ******");
	osd_info("PID: %u OID: %u", (uint)pid, (uint)oid);

	if (attrs) {
		osd_command_set_set_attributes(&command, pid, oid);
		command.outdata = attrs;
		command.outlen = sizeof(attrs); 

		ret = osd_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else 
		osd_error("%s: no attributes sent", __func__); 

	return 0;
}

int set_member_attributes(int fd, uint64_t pid, uint64_t cid, const struct attribute_id *attrs)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** SET MEMBER ATTRIBUTES ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

	if (attrs) {
		osd_command_set_set_member_attributes(&command, pid, cid);
		command.outdata = attrs;
		command.outlen = sizeof(attrs); 

		ret = osd_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else 
		osd_error("%s: no attributes sent", __func__); 

	return 0;
}

int get_member_attributes(int fd, uint64_t pid, uint64_t cid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** GET MEMBER ATTRIBUTES ******");
	osd_info("PID: %u CID: %u EMPTY BUF: <<%s>>", (uint)pid, (uint)cid, buf);

	osd_command_set_get_member_attributes(&command, pid, cid);
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

/* List */
int object_list(int fd, uint64_t pid, uint32_t list_id, uint64_t initial_oid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** LIST ******");
	osd_info("PID: %u LIST_ID: %u INITIAL_OID: %u EMPTY BUF: <<%s>>", 
		(uint)pid, (uint)list_id, (uint)initial_oid, buf);

	osd_command_set_list(&command, pid, list_id, sizeof(buf), initial_oid);
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);
	
	buf[0] = '\0';

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

int collection_list(int fd, uint64_t pid, uint64_t cid, uint32_t list_id, uint64_t initial_oid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** LIST COLLECTION ******");
	osd_info("PID: %u CID: %u LIST_ID: %u INITIAL_OID: %u EMPTY BUF: <<%s>>", 
		(uint)pid, (uint)cid, (uint)list_id, (uint)initial_oid, buf);

	osd_command_set_list_collection(&command, pid, cid, list_id, sizeof(buf), initial_oid);
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);
	
	buf[0] = '\0';

	ret = osd_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

