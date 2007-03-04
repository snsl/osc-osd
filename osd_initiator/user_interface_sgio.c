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
#include "user_interface_sgio.h"
#include "diskinfo.h"
#include "cdb_manip.h"

static const int INQUIRY_CDB_LEN = 6;
static const int INQUIRY_RSP_LEN = 80;

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

int inquiry_sgio(int fd)
{
	int ret;
	uint8_t inquiry_rsp[INQUIRY_RSP_LEN];
	struct osd_command command;

	osd_info("****** INQUIRY ******");

	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));
	memset(&command, 0, sizeof(command));
	cdb_build_inquiry(command.cdb, sizeof(inquiry_rsp));
	command.cdb_len = INQUIRY_CDB_LEN;
	command.indata = inquiry_rsp;
	command.inlen_alloc = sizeof(inquiry_rsp);

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

	osd_hexdump(inquiry_rsp, command.inlen);

	printf("\n");
	return 0;
}

int query_sgio(int fd, uint64_t pid, uint64_t cid, const char *query)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;
	
	osd_info("****** QUERY ******");
	osd_info("PID: %u CID: %u QUERY: %s", (uint)pid, (uint)cid, query);

	if (query) {
 		memset(&command, 0, sizeof(command));
		set_cdb_osd_query(command.cdb, pid, cid, strlen(query), sizeof(buf));
		command.cdb_len = OSD_CDB_SIZE;
		command.outdata = query;
		command.outlen = strlen(query) + 1;
		command.indata = buf;
		command.inlen_alloc = sizeof(buf);

		ret = osd_sgio_submit_and_wait(fd, &command);
		check_response(ret, command, buf);
	}
	else 
		osd_error("%s: no query sent", __func__); 

	return 0;
}
 
int create_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects)
{
	int ret, i;
	struct osd_command command;

	osd_info("****** CREATE OBJECT ******");
	osd_info("Creating %u objects", (uint)num_user_objects);

	for (i=0; i < num_user_objects; i++) {
		osd_info("PID: %u OID: %u OBJ: %u/%u", 
			(uint)pid, (uint)requested_oid + i, i+1, num_user_objects);

		memset(&command, 0, sizeof(command));

		/* Create user objects one at a time */ 
		set_cdb_osd_create(command.cdb, pid, requested_oid+i, 1);
		command.cdb_len = OSD_CDB_SIZE;

		ret = osd_sgio_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}

	return 0;
}

int create_partition_sgio(int fd, uint64_t requested_pid)
{
	int ret;
	struct osd_command command;

	osd_info("****** CREATE PARTITION ******");
	osd_info("PID: %u", (uint)requested_pid);

	memset(&command, 0, sizeof(command));

	set_cdb_osd_create_partition(command.cdb, requested_pid);
	command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

	return 0;
}

int create_collection_sgio(int fd, uint64_t pid, uint64_t requested_cid)
{
	int ret;
	struct osd_command command;

	osd_info("****** CREATE COLLECTION ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)requested_cid);

	memset(&command, 0, sizeof(command));
	set_cdb_osd_create_collection(command.cdb, pid, requested_cid);
	command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);
	return 0;
}

int remove_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE OBJECT ******");
	osd_info("PID: %u OID: %u", (uint)pid, (uint)requested_oid);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_remove(command.cdb, pid, requested_oid);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int remove_partition_sgio(int fd, uint64_t pid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE PARTITION ******");
	osd_info("PID: %u", (uint)pid);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_remove_partition(command.cdb, pid);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int remove_collection_sgio(int fd, uint64_t pid, uint64_t cid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE COLLECTION ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_remove_collection(command.cdb, pid, cid);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int remove_member_objects_sgio(int fd, uint64_t pid, uint64_t cid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE MEMBER OBJECTS ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_remove_member_objects(command.cdb, pid, cid);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0;
}

int create_osd_and_write_sgio(int fd, uint64_t pid, uint64_t requested_oid, const char *buf, uint64_t offset)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** CREATE / WRITE ******");
	osd_info("PID: %u OID: %u BUF: %s", (uint)pid, (uint)requested_oid, buf);

	if (buf) {
		memset(&command, 0, sizeof(command));
		set_cdb_osd_create_and_write(command.cdb, pid, requested_oid, strlen(buf) + 1, offset);
		command.cdb_len = OSD_CDB_SIZE;
		command.outdata = buf;
		command.outlen = strlen(buf) + 1;
	
		ret = osd_sgio_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else
		osd_error("%s: no data sent", __func__);

	return 0;
}

int write_osd_sgio(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** WRITE ******");
	osd_info("PID: %u OID: %u BUF: %s", (uint)pid, (uint)oid, buf);

	if (buf) {
 		memset(&command, 0, sizeof(command));
		set_cdb_osd_write(command.cdb, pid, oid, strlen(buf) + 1, offset);
		command.cdb_len = OSD_CDB_SIZE;
		command.outdata = buf;
		command.outlen = strlen(buf) + 1;

		ret = osd_sgio_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else 
		osd_error("%s: no data sent", __func__); 

	return 0;
}

int read_osd_sgio(int fd, uint64_t pid, uint64_t oid, uint64_t offset)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** READ ******");
	osd_info("PID: %u OID: %u EMPTY BUF: <<%s>>", (uint)pid, (uint)oid, buf);

	memset(&command, 0, sizeof(command));
	set_cdb_osd_read(command.cdb, pid, oid, sizeof(buf), offset);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

int format_osd_sgio(int fd, int capacity)
{
        int ret;
        struct osd_command command;

        osd_info("****** FORMAT ******"); 
	osd_info("Capacity: %i", capacity);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_format_osd(command.cdb, capacity);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);	
	check_response(ret, command, NULL);
	
        return 0;
}

int flush_osd_sgio(int fd, int flush_scope)
{
        int ret;
        struct osd_command command;

        osd_info("****** FLUSH OBJECT ******"); 

	memset(&command, 0, sizeof(command));
        set_cdb_osd_flush_osd(command.cdb, flush_scope);   
        command.cdb_len = OSD_CDB_SIZE;
	
	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0; 
}

int flush_partition_sgio(int fd, uint64_t pid, int flush_scope)
{
        int ret;
        struct osd_command command;

        osd_info("****** FLUSH PARTITION ******"); 
	osd_info("PID: %u", (uint)pid);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_flush_partition(command.cdb, pid, flush_scope);   
        command.cdb_len = OSD_CDB_SIZE;
	
	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0; 
}

int flush_collection_sgio(int fd, uint64_t pid, uint64_t cid, int flush_scope)
{
        int ret;
        struct osd_command command;

        osd_info("****** FLUSH COLLECTION ******"); 
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_flush_collection(command.cdb, pid, cid, flush_scope);   
        command.cdb_len = OSD_CDB_SIZE;
	
	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, NULL);

        return 0; 
}

int get_attributes_sgio(int fd, uint64_t pid, uint64_t oid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** GET ATTRIBUTES ******");
	osd_info("PID: %u OID: %u EMPTY BUF: <<%s>>", (uint)pid, (uint)oid, buf);

	memset(&command, 0, sizeof(command));
	set_cdb_osd_get_attributes(command.cdb, pid, oid);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

int get_attribute_page_sgio(int fd, uint64_t page, uint64_t offset)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** GET ATTRIBUTE PAGE ******");
	osd_info("PAGE: %u EMPTY BUF: <<%s>>", (uint)page, buf);

	memset(&command, 0, sizeof(command));
	set_cdb_get_attr_page(command.cdb, page, sizeof(buf), offset);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

int set_attributes_sgio(int fd, uint64_t pid, uint64_t oid, const struct attribute_id *attrs)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** SET ATTRIBUTES ******");
	osd_info("PID: %u OID: %u", (uint)pid, (uint)oid);

	if (attrs) {
 		memset(&command, 0, sizeof(command));
		set_cdb_osd_set_attributes(command.cdb, pid, oid);
		command.cdb_len = OSD_CDB_SIZE;
		command.outdata = attrs;
		command.outlen = sizeof(attrs); 

		ret = osd_sgio_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else 
		osd_error("%s: no attributes sent", __func__); 

	return 0;
}

int set_member_attributes_sgio(int fd, uint64_t pid, uint64_t cid, const struct attribute_id *attrs)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** SET MEMBER ATTRIBUTES ******");
	osd_info("PID: %u CID: %u", (uint)pid, (uint)cid);

	if (attrs) {
 		memset(&command, 0, sizeof(command));
		set_cdb_osd_set_member_attributes(command.cdb, pid, cid);
		command.cdb_len = OSD_CDB_SIZE;
		command.outdata = attrs;
		command.outlen = sizeof(attrs); 

		ret = osd_sgio_submit_and_wait(fd, &command);
		check_response(ret, command, NULL);
	}
	else 
		osd_error("%s: no attributes sent", __func__); 

	return 0;
}

int get_member_attributes_sgio(int fd, uint64_t pid, uint64_t cid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** GET MEMBER ATTRIBUTES ******");
	osd_info("PID: %u CID: %u EMPTY BUF: <<%s>>", (uint)pid, (uint)cid, buf);

	memset(&command, 0, sizeof(command));
	set_cdb_osd_get_member_attributes(command.cdb, pid, cid);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

/* List */
int object_list_sgio(int fd, uint64_t pid, uint32_t list_id, uint64_t initial_oid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** OBJECT LIST ******");
	osd_info("PID: %u LIST_ID: %u INITIAL_OID: %u EMPTY BUF: <<%s>>", 
		(uint)pid, (uint)list_id, (uint)initial_oid, buf);

	memset(&command, 0, sizeof(command));
	set_cdb_osd_list(command.cdb, pid, list_id, sizeof(buf), initial_oid);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);
	
	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

int collection_list_sgio(int fd, uint64_t pid, uint64_t cid, uint32_t list_id, uint64_t initial_oid)
{
	int ret;
	uint8_t buf[100] = "";
	struct osd_command command;

	osd_info("****** COLLECTION LIST ******");
	osd_info("PID: %u CID: %u LIST_ID: %u INITIAL_OID: %u EMPTY BUF: <<%s>>", 
		(uint)pid, (uint)cid, (uint)list_id, (uint)initial_oid, buf);

	memset(&command, 0, sizeof(command));
	set_cdb_osd_list_collection(command.cdb, pid, cid, list_id, sizeof(buf), initial_oid);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);
	
	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);
	check_response(ret, command, buf);

	return 0;
}

/*
 * Assume an empty command with no data, no retrieved offset, etc.
 * Build an attributes list, including allocating in and out space,
 * and alter the CDB.
 */
int osd_command_attr_build(struct osd_command *command,
                           struct attribute_id *attrs, int num)
{
    const uint32_t list_offset = 0, retrieved_offset = 0;
    uint32_t list_len, alloc_len;
    uint8_t *attr_list;
    int i;

    /* must fit in 2-byte list length field */
    if (num * 8 >= (1 << 16))
	return -EOVERFLOW;

    /*
     * Build the list that requests the attributes
     */
    list_len = 4 + num * 8;
    command->outlen = list_len;
    attr_list = malloc(command->inlen_alloc);
    if (!attr_list)
    	return -ENOMEM;
    command->outdata = attr_list;

    attr_list[0] = 0x1;  /* list type: retrieve attributes */
    attr_list[1] = 0;
    set_htons(&attr_list[2], num*8);
    for (i=0; i<num; i++) {
	set_htonl(&attr_list[4 + i*8 + 0], attrs[i].page);
	set_htonl(&attr_list[4 + i*8 + 4], attrs[i].number);
    }

    /*
     * Allocate space for where they will end up when returned.  Apparently
     * no padding here, just squeezed together exactly, with a 10-byte
     * header on each one.  Whole thing is preceded by the usual 4-byte
     * Table 126 list header.
     */
    alloc_len = 4;
    for (i=0; i<num; i++)
	alloc_len += 10 + attrs[i].len;

    command->inlen_alloc = alloc_len;
    command->indata = malloc(command->inlen_alloc);
    if (!command->indata)
	return -ENOMEM;

    /* Set the CDB bits to point appropriately. */
    set_cdb_get_attr_list(command->cdb, list_len, list_offset, alloc_len,
                          retrieved_offset);

    return 0;
}

/*
 * Return a pointer to the returned attribute data.  Maybe do verify as
 * a separate test and have this just be quick.
 */
uint8_t *osd_command_attr_resolve(struct osd_command *command,
                                  struct attribute_id *attrs, int num,
			          int index)
{
	uint32_t list_len;
	uint8_t *p;
	int i;

	p = command->indata;
	list_len = 4;
	for (i=0; i<num; i++)
		list_len += 10 + attrs[i].len;

	if (command->inlen != list_len) {
		osd_error("%s: expecting %u bytes, got %zu, [0] = %02x",
		          __func__, list_len, command->inlen, p[0]);
		return NULL;
	}
	if ((p[0] & 0xf) != 0x9) {
		osd_error("%s: expecting list type 9, got 0x%x", __func__,
		          p[0] & 0xf);
		return NULL;
	}
	if (ntohs(&p[2]) != command->inlen - 4) {
		osd_error("%s: expecting list length %zu, got %u", __func__,
		          command->inlen - 4, ntohs(&p[2]));
		return NULL;
	}

	p += 4;
	for (i=0; i<num; i++) {
		if (ntohl(&p[0]) != attrs[i].page) {
			osd_error("%s: expecting page %x, got %x", __func__,
				  attrs[i].page, ntohl(&p[0]));
			return NULL;
		}
		if (ntohl(&p[4]) != attrs[i].number) {
			osd_error("%s: expecting number %x, got %x", __func__,
				  attrs[i].page, ntohl(&p[4]));
			return NULL;
		}
		if (ntohs(&p[8]) != attrs[i].len) {
			osd_error("%s: expecting length %u, got %u", __func__,
				  attrs[i].len, ntohs(&p[8]));
			return NULL;
		}
	    	if (i == index)
			return &p[10];
	}
	osd_error("%s: attribute %d out of range", __func__, index);
	return NULL;
}

