#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <scsi/sg.h>

#include <stdint.h>
#include <sys/types.h>

#include "util/util.h"

#include "command.h"
#include "osd_cmds.h"

#define OSD_CDB_SIZE 200
#define VARLEN_CDB 0x7f
#define TIMESTAMP_ON 0x0
#define TIMESTAMP_OFF 0x7f

/* cdb initialization / manipulation functions */

static void varlen_cdb_init(struct osd_command *command, uint16_t action)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = OSD_CDB_SIZE;
	command->cdb[0] = VARLEN_CDB;
	/* we do not support ACA or LINK in control byte cdb[1], leave as 0 */
	command->cdb[7] = OSD_CDB_SIZE - 8;
        command->cdb[8] = (action & 0xff00U) >> 8;
        command->cdb[9] = (action & 0x00ffU);
	command->cdb[11] = 2 << 4;  /* a default, even if no attrs */
	/* Update timestamps based on action 5.2.8 */
	command->cdb[12] = TIMESTAMP_OFF;
}

/*
 * Non-varlen commands.
 */
int osd_command_set_inquiry(struct osd_command *command, uint8_t outlen)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = 6;
	memset(command->cdb, 0, 6);
	command->cdb[0] = INQUIRY;
	command->cdb[4] = outlen;
	return 0;
}

/*
 * These functions take a cdb of the appropriate size (200 bytes),
 * and the arguments needed for the particular command.  They marshall
 * the arguments but do not submit the command.
 */     
int osd_command_set_append(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_APPEND);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        return 0;
}


int osd_command_set_create(struct osd_command *command, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htons(&command->cdb[36], num_user_objects);
        return 0;
}


int osd_command_set_create_and_write(struct osd_command *command, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE_AND_WRITE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}


int osd_command_set_create_collection(struct osd_command *command, uint64_t pid, uint64_t requested_cid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_cid);
        return 0;
}

int osd_command_set_create_partition(struct osd_command *command, uint64_t requested_pid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE_PARTITION);
        set_htonll(&command->cdb[16], requested_pid);
        return 0;
}


int osd_command_set_flush(struct osd_command *command, uint64_t pid, uint64_t oid, int flush_scope)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FLUSH);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_collection(struct osd_command *command, uint64_t pid, uint64_t cid,
                         int flush_scope)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FLUSH_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_osd(struct osd_command *command, int flush_scope)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FLUSH_OSD);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_partition(struct osd_command *command, uint64_t pid, int flush_scope)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FLUSH_PARTITION);
        set_htonll(&command->cdb[16], pid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}

/*
 * Destroy the db and start over again.
 */
int osd_command_set_format_osd(struct osd_command *command, uint64_t capacity)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FORMAT_OSD);
        set_htonll(&command->cdb[36], capacity);
        return 0;
}


int osd_command_set_get_attributes(struct osd_command *command, uint64_t pid, uint64_t oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_GET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_get_member_attributes(struct osd_command *command, uint64_t pid, uint64_t cid) /*section in spec?*/
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_GET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_list(struct osd_command *command, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_LIST);
        set_htonll(&command->cdb[16], pid);
        set_htonl(&command->cdb[32], list_id);
        set_htonll(&command->cdb[36], alloc_len);
        set_htonll(&command->cdb[44], initial_oid);
        return 0;
}

int osd_command_set_list_collection(struct osd_command *command, uint64_t pid, uint64_t cid,  /*section in spec?*/
                        uint32_t list_id, uint64_t alloc_len,
                        uint64_t initial_oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_LIST_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        set_htonl(&command->cdb[32], list_id);
        set_htonll(&command->cdb[36], alloc_len);
        set_htonll(&command->cdb[44], initial_oid);
        return 0;
}

int osd_command_set_perform_scsi_command(struct osd_command *command __unused)
{
        osd_error("%s: unimplemented", __func__);
        return 1;
}

int osd_command_set_perform_task_mgmt_func(struct osd_command *command __unused)
{
        osd_error("%s: unimplemented", __func__);
        return 1;
}

int osd_command_set_query(struct osd_command *command, uint64_t pid, uint64_t cid, uint32_t query_len,    /*section in spec?*/
              uint64_t alloc_len)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_QUERY);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        set_htonl(&command->cdb[32], query_len);
        set_htonll(&command->cdb[36], alloc_len);
        return 0;
}

int osd_command_set_read(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len,
             uint64_t offset)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_READ);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}


int osd_command_set_remove(struct osd_command *command, uint64_t pid, uint64_t oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_REMOVE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_remove_collection(struct osd_command *command, uint64_t pid, uint64_t cid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_REMOVE_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_member_objects(struct osd_command *command, uint64_t pid, uint64_t cid) /*section in spec?*/
{
        varlen_cdb_init(command, OSD_REMOVE_MEMBER_OBJECTS);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_partition(struct osd_command *command, uint64_t pid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_REMOVE_PARTITION);
        set_htonll(&command->cdb[16], pid);
        return 0;
}


int osd_command_set_set_attributes(struct osd_command *command, uint64_t pid, uint64_t oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_set_key(struct osd_command *command, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20])
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | key_to_set;
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], key);
        memcpy(&command->cdb[32], seed, 20);
        return 0;
}


int osd_command_set_set_master_key(struct osd_command *command, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | dh_step;
        set_htonll(&command->cdb[24], key);
        set_htonl(&command->cdb[32], param_len);
        set_htonl(&command->cdb[36], alloc_len);
        return 0;
}


int osd_command_set_set_member_attributes(struct osd_command *command, uint64_t pid, uint64_t cid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_write(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len,
              uint64_t offset)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_WRITE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}


/*
 * Attribute list get/set functions.
 * XXX: the offsets here should be of weird offset type.
 */
void set_cdb_get_attr_page(struct osd_command *command, uint32_t page, uint32_t len,
                           uint32_t retrieved_offset)
{
	command->cdb[11] = (command->cdb[11] & ~(3 << 4)) | (2 << 4);
	set_htonl(&command->cdb[52], page);
	set_htonl(&command->cdb[56], len);
	set_htonl(&command->cdb[60], retrieved_offset);
}

/*
 * Attribute list get/set functions.
 */

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
	list_len = 8 + num * 8;
	command->outlen = list_len;
	attr_list = malloc(command->inlen_alloc);
	if (!attr_list)
	    return -ENOMEM;
	command->outdata = attr_list;

	attr_list[0] = 0x1;  /* list type: retrieve attributes */
	attr_list[1] = 0;
	attr_list[2] = 0;
	attr_list[3] = 0;
	set_htonl(&attr_list[4], num*8);
	for (i=0; i<num; i++) {
	    set_htonl(&attr_list[8 + i*8 + 0], attrs[i].page);
	    set_htonl(&attr_list[8 + i*8 + 4], attrs[i].number);
	}

	/*
	 * Allocate space for where they will end up when returned.  Entries
	 * are padded to 8 bytes.
	 */
	alloc_len = 8;
	for (i=0; i<num; i++)
	    alloc_len += (10 + attrs[i].len + 7) & ~7;

	command->inlen_alloc = alloc_len;
	command->indata = malloc(command->inlen_alloc);
	if (!command->indata)
	    return -ENOMEM;

	/* Set the CDB bits to point appropriately: list format */
	command->cdb[11] = command->cdb[11] | (3 << 4);
	set_htonl(&command->cdb[52], list_len);
	set_htonl(&command->cdb[56], list_offset);
	set_htonl(&command->cdb[60], alloc_len);
	set_htonl(&command->cdb[64], retrieved_offset);

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
	list_len = 8;
	for (i=0; i<num; i++)
		list_len += (10 + attrs[i].len + 7) & ~7;

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
	if (ntohl(&p[4]) != command->inlen - 8) {
		osd_error("%s: expecting list length %zu, got %u", __func__,
		          command->inlen - 8, ntohl(&p[4]));
		return NULL;
	}

	p += 8;
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
		p += (10 + attrs[i].len + 7) & ~7;
	}
	osd_error("%s: attribute %d out of range", __func__, index);
	return NULL;
}

