/*
 * Commands and attributes.
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
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <scsi/scsi.h>

/* turn this on automatically with configure some day */
#if 0
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_DEFINED(p, len)
#endif

/* #define BSG_BACK_TO_SG_IOVEC */

#ifdef BSG_BACK_TO_SG_IOVEC
#include <scsi/sg.h>  /* sg_iovec */
#endif

#include "osd-util/osd-util.h"
#include "command.h"



static void varlen_cdb_init(struct osd_command *command, uint16_t action)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = OSD_CDB_SIZE;
	command->cdb[0] = VARLEN_CDB;
	/* we do not support ACA or LINK in control byte cdb[1], leave as 0 */
	command->cdb[7] = OSD_CDB_SIZE - 8;
        command->cdb[8] = (action & 0xff00U) >> 8;
        command->cdb[9] = (action & 0x00ffU);
	/* default data distribution type is contiguous (0) */
	command->cdb[11] = 3 << 4;  /* default to list, but empty list lens */
	/* Update timestamps based on action 5.2.8 */
	command->cdb[12] = 0x7f;  /* timestamps not updated */
}

/*
 * Non-varlen commands.
 */
int osd_command_set_test_unit_ready(struct osd_command *command)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = 6;
	return 0;
}

int osd_command_set_inquiry(struct osd_command *command, uint8_t page_code,
			    uint8_t outlen)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = 6;
	command->cdb[0] = INQUIRY;
	command->cdb[4] = outlen;
	if (page_code) {
		command->cdb[1] = 1;
		command->cdb[2] = page_code;
	}
	return 0;
}

/*
 * These functions take a cdb of the appropriate size (200 bytes),
 * and the arguments needed for the particular command.  They marshall
 * the arguments but do not submit the command.
 */
int osd_command_set_append(struct osd_command *command, uint64_t pid,
			   uint64_t oid, uint64_t len)
{
        varlen_cdb_init(command, OSD_APPEND);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[32], len);
        return 0;
}

int osd_command_set_clear(struct osd_command *command, uint64_t pid,
			   uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_CLEAR);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[32], len);
	set_htonll(&command->cdb[40], offset);
        return 0;
}

int osd_command_set_create(struct osd_command *command, uint64_t pid,
			   uint64_t requested_oid, uint16_t num_user_objects)
{
        varlen_cdb_init(command, OSD_CREATE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htons(&command->cdb[32], num_user_objects);
        return 0;
}


int osd_command_set_create_and_write(struct osd_command *command, uint64_t pid,
				     uint64_t requested_oid, uint64_t len,
				     uint64_t offset)
{
        varlen_cdb_init(command, OSD_CREATE_AND_WRITE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htonll(&command->cdb[32], len);
        set_htonll(&command->cdb[40], offset);
        return 0;
}


int osd_command_set_create_collection(struct osd_command *command,
				      uint64_t pid, uint64_t requested_cid)
{
        varlen_cdb_init(command, OSD_CREATE_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_cid);
        return 0;
}

int osd_command_set_create_partition(struct osd_command *command,
				     uint64_t requested_pid)
{
        varlen_cdb_init(command, OSD_CREATE_PARTITION);
        set_htonll(&command->cdb[16], requested_pid);
        return 0;
}


int osd_command_set_flush(struct osd_command *command, uint64_t pid, uint64_t len,
			  uint64_t offset, uint64_t oid, int flush_scope)
{
        varlen_cdb_init(command, OSD_FLUSH);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
	set_htonll(&command->cdb[32], len);
	set_htonll(&command->cdb[40], offset);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_collection(struct osd_command *command, uint64_t pid,
				     uint64_t cid, int flush_scope)
{
        varlen_cdb_init(command, OSD_FLUSH_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_osd(struct osd_command *command, int flush_scope)
{
        varlen_cdb_init(command, OSD_FLUSH_OSD);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_partition(struct osd_command *command, uint64_t pid,
				    int flush_scope)
{
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
        varlen_cdb_init(command, OSD_FORMAT_OSD);
        set_htonll(&command->cdb[32], capacity);
        return 0;
}


int osd_command_set_get_attributes(struct osd_command *command, uint64_t pid,
				   uint64_t oid)
{
        varlen_cdb_init(command, OSD_GET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_get_member_attributes(struct osd_command *command,
					  uint64_t pid, uint64_t cid)
{
        varlen_cdb_init(command, OSD_GET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_list(struct osd_command *command, uint64_t pid,
			 uint32_t list_id, uint64_t alloc_len,
			 uint64_t initial_oid, int list_attr)
{
        varlen_cdb_init(command, OSD_LIST);
	if (list_attr)
		command->cdb[11] |= 0x40;
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[32], alloc_len);
        set_htonll(&command->cdb[40], initial_oid);
        set_htonl(&command->cdb[48], list_id);
        return 0;
}

int osd_command_set_list_collection(struct osd_command *command, uint64_t pid,
				    uint64_t cid, uint32_t list_id,
				    uint64_t alloc_len, uint64_t initial_oid,
				    int list_attr)
{
        varlen_cdb_init(command, OSD_LIST_COLLECTION);
	if (list_attr)
		command->cdb[11] |= 0x40;
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        set_htonll(&command->cdb[32], alloc_len);
        set_htonll(&command->cdb[40], initial_oid);
        set_htonl(&command->cdb[48], list_id);
        return 0;
}

int osd_command_set_perform_scsi_command(
			struct osd_command *command __attribute__((unused)))
{
        osd_error("%s: unimplemented", __func__);
        return 1;
}

int osd_command_set_perform_task_mgmt_func(
			struct osd_command *command __attribute__((unused)))
{
        osd_error("%s: unimplemented", __func__);
        return 1;
}

int osd_command_set_punch(struct osd_command *command, uint64_t pid,
			  uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_PUNCH);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[32], len);
        set_htonll(&command->cdb[40], offset);
        return 0;
}

int osd_command_set_query(struct osd_command *command, uint64_t pid,
			  uint64_t cid, uint32_t query_len, uint64_t alloc_len)
{
        varlen_cdb_init(command, OSD_QUERY);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        set_htonl(&command->cdb[48], query_len);
        set_htonll(&command->cdb[32], alloc_len);
        return 0;
}

int osd_command_set_read(struct osd_command *command, uint64_t pid,
			 uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_READ);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[32], len);
        set_htonll(&command->cdb[40], offset);
        return 0;
}

int osd_command_set_read_map(struct osd_command*command, uint64_t pid, uint64_t oid,
			 uint64_t alloc_len, uint64_t offset, uint8_t map_type)
{
        varlen_cdb_init(command, OSD_READ_MAP);
	set_htonll(&command->cdb[16], pid);
	set_htonll(&command->cdb[24], oid);
	set_htonll(&command->cdb[32], alloc_len);
	set_htonll(&command->cdb[40], offset);
	set_htons(&command->cdb[48], map_type);
	return 0;
}


int osd_command_set_remove(struct osd_command *command, uint64_t pid,
			   uint64_t oid)
{
        varlen_cdb_init(command, OSD_REMOVE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_remove_collection(struct osd_command *command,
				      uint64_t pid, uint64_t cid, int force)
{
        varlen_cdb_init(command, OSD_REMOVE_COLLECTION);
	if (force)
		command->cdb[11] |= 1;
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_member_objects(struct osd_command *command,
					  uint64_t pid, uint64_t cid)
{
        varlen_cdb_init(command, OSD_REMOVE_MEMBER_OBJECTS);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_partition(struct osd_command *command, uint64_t pid)
{
        varlen_cdb_init(command, OSD_REMOVE_PARTITION);
        set_htonll(&command->cdb[16], pid);
        return 0;
}


int osd_command_set_set_attributes(struct osd_command *command, uint64_t pid,
				   uint64_t oid)
{
        varlen_cdb_init(command, OSD_SET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_set_key(struct osd_command *command, int key_to_set,
			    uint64_t pid, uint64_t key, const uint8_t seed[20])
{
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | key_to_set;
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], key);
        memcpy(&command->cdb[32], seed, 20);
        return 0;
}


int osd_command_set_set_master_key(struct osd_command *command, int dh_step,
				   uint64_t key, uint32_t param_len,
				   uint32_t alloc_len)
{
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | dh_step;
        set_htonll(&command->cdb[24], key);
        set_htonl(&command->cdb[32], param_len);
        set_htonl(&command->cdb[36], alloc_len);
        return 0;
}


int osd_command_set_set_member_attributes(struct osd_command *command,
					  uint64_t pid, uint64_t cid)
{
        varlen_cdb_init(command, OSD_SET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_write(struct osd_command *command, uint64_t pid,
			  uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_WRITE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[32], len);
        set_htonll(&command->cdb[40], offset);
        return 0;
}

int osd_command_set_cas(struct osd_command *command, uint64_t pid,
			uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_CAS);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[32], len);
        set_htonll(&command->cdb[40], offset);
        return 0;
}

int osd_command_set_fa(struct osd_command *command, uint64_t pid,
			uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_FA);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[32], len);
        set_htonll(&command->cdb[40], offset);
        return 0;
}

int osd_command_set_gen_cas(struct osd_command *command, uint64_t pid,
			    uint64_t oid)
{
        varlen_cdb_init(command, OSD_GEN_CAS);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
	return 0;
}

int osd_command_set_cond_setattr(struct osd_command *command, uint64_t pid,
				 uint64_t oid)
{
        varlen_cdb_init(command, OSD_COND_SETATTR);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
	return 0;
}

void osd_command_set_ddt(struct osd_command *command, uint8_t ddt_type)
{
	/*
	 * Using last 2 bits of option byte, just like flush_scope, see 5.2.5
	 */
        command->cdb[10] = (command->cdb[10] & ~0x3) | ddt_type;
}


uint8_t osd_command_get_ddt(struct osd_command *command)
{
	return command->cdb[10] & 0x3;
}

/*
 * Header for internal use across attr_build to attr_resolve.  Keeps
 * the original iov structures.
 */
struct attr_malloc_header {
	const void *orig_outdata;
	size_t orig_outlen;
	int orig_iov_outlen;
	void *orig_indata;
	size_t orig_inlen_alloc;
	int orig_iov_inlen;
	uint8_t *retrieved_buf;
	uint64_t start_retrieved;
	/* for convenience of resolve, points into the big buffer */
	struct attribute_get_multi_results *mr;
};

/*
 * After setting the command (i.e. building the CDB), and connecting
 * the indata and outdata, this function tacks on a list of attributes.
 * To do this it will possibly create a new buffer for output data
 * containing the request, and it will possibly create a new buffer for
 * input data with the GET responses.  And it will build an iovec to
 * hold the command data as well as the attribute data.
 *
 * After the command completes, call the resolve function to fill in
 * the results and free the temp buffers.
 */
int osd_command_attr_build(struct osd_command *command,
                           const struct attribute_list *attr, int numattr)
{
        int use_getpage, use_set_one_attr;
	int numget, numgetpage, numgetmulti, numset, numresult;
	uint32_t getsize, getpagesize, getmultisize, setsize, resultsize;
	uint32_t getmulti_num_objects = 0;
	struct attr_malloc_header *header;
	uint8_t *p, *extra_out_buf, *extra_in_buf;
#ifdef BSG_BACK_TO_SG_IOVEC
	struct sg_iovec *iov;
#else
	struct bsg_iovec *iov;
#endif
	int i;
	int neediov;
	int setattr_index;
	uint64_t extra_out, extra_in;
	uint32_t header_space, attr_space, iov_space, getmulti_result_space;

	/*
	 * These are values, in units of bytes (not offsets), in the
	 * out data.
	 */
	uint64_t end_outdata;
	uint64_t size_pad_outdata;
	uint64_t start_getlist;
	uint64_t size_getlist;
	uint64_t size_pad_setlist;
	uint64_t start_setlist;
	uint64_t size_setlist;

	/*
	 * Ditto, in data.
	 */
	uint64_t end_indata;
	uint64_t size_pad_indata;
	uint64_t start_retrieved;
	uint64_t size_retrieved;

	/*
	 * These are used so that we know the setlist and retrieved
	 * areas start on 8-byte boundaries.  Since padding after
	 * outdata or indata can be arbitrary, alloc a bit extra to
	 * keep the pointers nice.
	 */
	uint64_t size_pad_outdata_alloc;
	uint64_t size_pad_indata_alloc;
	uint64_t extra_out_alloc, extra_in_alloc;
          		
	if (numattr == 0)
		return 0;

	/*
	 * Figure out what operations to do.  That will lead to constraints
	 * on the choice of getpage/setone versus getlist/setlist format.
	 */
	numget = numgetpage = numgetmulti = numset = numresult = 0;
	getsize = getpagesize = getmultisize = setsize = resultsize = 0;
	setattr_index = -1;
	for (i=0; i<numattr; i++) {
		if (attr[i].type == ATTR_GET) {
			++numget;
			getsize += roundup8(10 + attr[i].len);
		} else if (attr[i].type == ATTR_GET_PAGE) {
			++numgetpage;
			getpagesize += attr[i].len;  /* no round */
		} else if (attr[i].type == ATTR_GET_MULTI) {
			++numgetmulti;
			getmultisize += roundup8(18 + attr[i].len);
		} else if (attr[i].type == ATTR_SET) {
			++numset;
			setattr_index = i;
			setsize += roundup8(10 + attr[i].len);
		} else if (attr[i].type == ATTR_RESULT) {
			++numresult;
			resultsize += roundup8(10 + attr[i].len);
		} else {
			osd_error("%s: invalid attribute type %d", __func__,
			          attr[i].type);
			return -EINVAL;
		}
	}

	if (numgetpage) {
		if (numgetpage > 1) {
			osd_error("%s: only one ATTR_GET_PAGE at a time",
				  __func__);
			return -EINVAL;
		}
		if (numget) {
			osd_error("%s: no ATTR_GET allowed with ATTR_GET_PAGE",
				  __func__);
			return -EINVAL;
		}
		if (numgetmulti) {
			osd_error(
			    "%s: no ATTR_GETMULTI allowed with ATTR_GET_PAGE",
				  __func__);
			return -EINVAL;
		}
		if (numset > 1) {
			osd_error("%s: ATTR_GET_PAGE limits ATTR_SET to one",
				  __func__);
			return -EINVAL;
		}
		if (numresult) {
			/* Only CAS uses ATTR_RESULT, which needs list fmt */
			osd_error("%s: ATTR_RESULT not allowed with "
				  "ATTR_GET_PAGE", __func__);
			return -EINVAL;
		}
	        use_getpage = 1;

	}else {
		if (numgetmulti) {
			uint16_t action = (command->cdb[8] << 8)
			                 | command->cdb[9];
			if (numget) {
				osd_error(
				"%s: no ATTR_GET allowed with ATTR_GET_MULTI",
					  __func__);
				return -EINVAL;
			}
			if (action != OSD_CREATE) {
				osd_error(
			    "%s: ATTR_GET_MULTI must only be used with CREATE",
					  __func__);
				return -EINVAL;
			}
			getmulti_num_objects = get_ntohs(&command->cdb[36]);
		}
		use_getpage = 0;
		if (numget == 0 && numgetmulti == 0 && numset == 1) {
			/*
			 * For a single set and service action !=
			 * OSD_SET_MEMBER_ATTRIBUTES *, prefer simpler page
			 * format, * except if page-to-set is zero, which is
			 * a noop in * this format.
			 */
			uint8_t *cdb = command->cdb;
			uint16_t action = ((cdb[8] << 8) | cdb[9]);
			if (attr[setattr_index].page != 0 &&
			    (action != OSD_SET_MEMBER_ATTRIBUTES)) {
				use_getpage = 1;
			}
		}
		if (attr[0].len <= 18 && numset == 1 && numattr == 1)
		        use_set_one_attr = 1;
	}

	/*
	 * Outdata:
	 *	padding between real outdata and setlist
	 *	setlist (including values)
	 *	padding between setlist and getlist
	 *	getlist
	 */
	end_outdata = command->outlen;

	size_pad_outdata = 0;
	size_pad_outdata_alloc = 0;  /* so setlist starts on 8-byte boundary */
	start_setlist = 0;
	size_setlist = 0;
	if (numset) {
		start_setlist = next_offset(end_outdata);
		size_pad_outdata = start_setlist - end_outdata;
		size_pad_outdata_alloc = roundup8(size_pad_outdata);
		if (use_getpage)
			/* no list format around the single value */
			size_setlist = attr[setattr_index].len;
		else
			/* add list header; setsize already rounded */
			size_setlist = 8 + setsize;
	}

	size_pad_setlist = 0;
	start_getlist = 0;
	size_getlist = 0;
	if (numget || numgetmulti) {
		uint64_t prev = start_setlist + size_setlist;
		if (prev == 0)
			prev = end_outdata;
		start_getlist = next_offset(prev);
		size_pad_setlist = start_getlist - prev;
		if (size_setlist == 0)
			size_pad_outdata_alloc = roundup8(size_pad_setlist);
		size_getlist = 8 + 8 * (numget + numgetmulti);
	}

	extra_out = size_pad_outdata + size_setlist
		  + size_pad_setlist + size_getlist;
	extra_out_alloc = size_pad_outdata_alloc + size_setlist
		  + size_pad_setlist + size_getlist;

	/*
	 * Indata:
	 *	padding between real indata and retrieved results area
	 *	retrieved results area
	 */
	end_indata = command->inlen_alloc;
	start_retrieved = 0;
	size_pad_indata = 0;
	size_pad_indata_alloc = 0;
	size_retrieved = 0;
	if (numget || numgetpage || numgetmulti || numresult) {
		start_retrieved = next_offset(end_indata);
		size_pad_indata = start_retrieved - end_indata;
		size_pad_indata_alloc = roundup8(size_pad_indata);
		if (numget)
			size_retrieved = 8 + getsize;  /* already rounded */
		if (numgetpage)
			size_retrieved = getpagesize;  /* no rounding */
		if (numgetmulti)
			size_retrieved = 8 + getmultisize *
					 getmulti_num_objects;
		if (numresult) {
			if (!size_retrieved)
				size_retrieved += 8;
			size_retrieved += resultsize;
		}
	}

	extra_in = size_pad_indata + size_retrieved;
	extra_in_alloc = size_pad_indata_alloc + size_retrieved;

	/*
	 * Each ATTR_GET_MULTI will return an array of values, one for
	 * each object.  Allocate space for that now, for use by
	 * attr_resolve();
	 */
	getmulti_result_space = 0;
	if (numgetmulti) {
		struct attribute_get_multi_results *mr;
		int one_space = sizeof(*mr)
			+ getmulti_num_objects
			* (sizeof(*mr->oid) + sizeof(*mr->val)
			   + sizeof(*mr->outlen));
		getmulti_result_space = roundup8(numgetmulti * one_space);
	}

	/*
	 * If we are building out or indata items, and user already
	 * had some data there, build replacement iovecs to hold the
	 * new list.
	 */
	neediov = 0;
	if (extra_out && command->outdata) {
		if (command->iov_outlen == 0)
			neediov += 2;
		else
			neediov += command->iov_outlen + 1;
	}
	if (extra_in && command->indata) {
		if (command->iov_inlen == 0)
			neediov += 2;
		else
			neediov += command->iov_inlen + 1;
	}

	/*
	 * Allocate space for the following things, in order:
	 * 	header struct of original iov/data, etc.
	 * 	attr array
	 * 	get_multi results
	 * 	out iovec
	 * 	in iovec
	 * 	outgoing getlist area
	 * 	outgoing setlist area
	 * 	incoming retrieved area
	 */
	header_space = roundup8(sizeof(*header));
	attr_space = roundup8(numattr * sizeof(*attr));
	iov_space = roundup8(neediov * sizeof(*iov));
	p = Malloc(header_space
		 + attr_space
		 + getmulti_result_space
		 + iov_space
		 + extra_out_alloc
		 + extra_in_alloc);
	if (!p)
		return 1;

	/* to free it later */
	command->attr_malloc = p;

	/* header holds original indata, iniovec, etc. */
	header = (void *) p;
	p += header_space;

	/* copy user's passed-in attribute structure for use by resolve */
	command->attr = (void *) p;
	memcpy(command->attr, attr, numattr * sizeof(*attr));
	command->numattr = numattr;
	p += attr_space;

	/* for ATTR_GET_MULTI results */
	header->mr = (void *) p;
	p += getmulti_result_space;

	/* space for replacement out and in iovecs */
	iov = (void *) p;
	p += iov_space;

	/* extra out; including pad, setlist, pad, getlist */
	extra_out_buf = p;
	p += extra_out_alloc;

	/* extra in; including pad, retrieved area */
	extra_in_buf = p;

	/*
	 * Initialize get multi result space.
	 */
	if (getmulti_result_space) {
		struct attribute_get_multi_results *mr = header->mr;
		uint8_t *q = (void *) &mr[numgetmulti];
		for (i=0; i<numgetmulti; i++) {
			mr[i].numoid = 0;
			mr[i].oid = (void *) q;
			q += getmulti_num_objects * sizeof(*mr[i].oid);
			mr[i].val = (void *) q;
			q += getmulti_num_objects * sizeof(*mr[i].val);
			mr[i].outlen = (void *) q;
			q += getmulti_num_objects * sizeof(*mr[i].outlen);
		}
	}

	/*
	 * Start on outbuf.  Skip over 0..7 bytes used to pad indata
	 * that would wreck our setlist alignment.
	 */
	p = extra_out_buf + size_pad_outdata_alloc;

	/*
	 * Build the setlist, or copy in the single value to set.
	 */
	if (numset) {
		if (use_getpage) {
			memcpy(p, attr[setattr_index].val,
			       attr[setattr_index].len);
		} else {
			uint8_t *q = p + 8;
			for (i=0; i<numattr; i++) {
				int len;
				if (attr[i].type != ATTR_SET)
					continue;
				set_htonl(&q[0], attr[i].page);
				set_htonl(&q[4], attr[i].number);
				set_htons(&q[8], attr[i].len);
				memcpy(&q[10], attr[i].val, attr[i].len);
				len = 10 + attr[i].len;
				q += len;
				while (len & 7) {
					*q++ = 0;
					++len;
				}
			}
			/*
			 * osd2r3 7.1.3.1 says client should set list length
			 * to 0; value in CDB is what matters.
			 */
			memset(p, 0, 8);
			p[0] = 0x9;
		}
		p += size_setlist;
	}

	p += size_pad_setlist;

	/*
	 * Build the getlist.
	 */
	if (numget || numgetmulti) {
		uint8_t *q = p + 8;
		for (i=0; i<numattr; i++) {
			if (!(attr[i].type == ATTR_GET_MULTI
			   || attr[i].type == ATTR_GET))
				continue;
			set_htonl(&q[0], attr[i].page);
			set_htonl(&q[4], attr[i].number);
			q += 8;
		}
		memset(p, 0, 8);
		p[0] = 0x1;
		p += size_getlist;
	}
	/* no padding at the end of the getlist */

	/*
	 * Remember a pointer to the retrieved buffer (padded nicely)
	 * and the cdb offset for the retrieved.
	 */
	header->retrieved_buf = extra_in_buf + size_pad_indata_alloc;
	header->start_retrieved = start_retrieved;

	/*
	 * Update the CDB bits.
	 */
	if (use_set_one_attr) {
	        /* set one attribute */
	        command->cdb[11] = (command->cdb[11] & ~(3 << 4)) | (1 << 4);
		        set_htonl(&command->cdb[52], attr[0].page);
		        set_htonl(&command->cdb[56], attr[0].number);
			set_htons(&command->cdb[60], attr[0].len);
			memcpy(&command->cdb[62], attr[0].val, attr[0].len);
	}
	
	else if (use_getpage) {
		/* page format */
	        command->cdb[11] = (command->cdb[11] & ~(3 << 4)) | (2 << 4);
		for (i=0; i<numattr; i++) {
			if (attr[i].type == ATTR_GET_PAGE)
				set_htonl(&command->cdb[52], attr[i].page);
			if (attr[i].type == ATTR_SET) {
				set_htonl(&command->cdb[64], attr[i].page);
				set_htonl(&command->cdb[68], attr[i].number);
			}
		}
		set_htonl(&command->cdb[56], size_retrieved);
		set_htonoffset(&command->cdb[60], start_retrieved);
		set_htonl(&command->cdb[72], size_setlist);
		set_htonoffset(&command->cdb[76], start_setlist);
	} else {
		/* list format, cdb[11] is this as default */
		set_htonl(&command->cdb[52], size_getlist);
		set_htonoffset(&command->cdb[56], start_getlist);
		set_htonl(&command->cdb[60], size_retrieved);
		set_htonoffset(&command->cdb[64], start_retrieved);
		set_htonl(&command->cdb[68], size_setlist);
		set_htonoffset(&command->cdb[72], start_setlist);
	}

	/*
	 * Adjust the iovecs.  These use the real sizes, not the _alloc
	 * sizes.
	 */
	header->orig_outdata = command->outdata;
	header->orig_outlen = command->outlen;
	header->orig_iov_outlen = command->iov_outlen;
	if (extra_out) {
		/* ignore the 0..7 extra bytes so that data lands nicely */
		p = extra_out_buf + (extra_out_alloc - extra_out);
		if (command->outdata) {
			command->outdata = iov;
			if (command->iov_outlen == 0) {
				iov->iov_base =
					(uintptr_t) header->orig_outdata;
				iov->iov_len = command->outlen;
				++iov;
				command->iov_outlen = 2;
			} else {
				memcpy(iov, header->orig_outdata,
				       command->iov_outlen * sizeof(*iov));
				iov += command->iov_outlen;
				++command->iov_outlen;
			}
			iov->iov_base = (uintptr_t) p;
			iov->iov_len = extra_out;
			++iov;
		} else {
			command->outdata = p;
		}
		command->outlen += extra_out;
	}

	header->orig_indata = command->indata;
	header->orig_inlen_alloc = command->inlen_alloc;
	header->orig_iov_inlen = command->iov_inlen;
	if (extra_in) {
		p = extra_in_buf + (extra_in_alloc - extra_in);
		if (command->indata) {
			command->indata = iov;
			if (command->iov_inlen == 0) {
				iov->iov_base = (uintptr_t) header->orig_indata;
				iov->iov_len = command->inlen_alloc;
				++iov;
				command->iov_inlen = 2;
			} else {
				memcpy(iov, header->orig_indata,
				       command->iov_inlen * sizeof(*iov));
				iov += command->iov_inlen;
				++command->iov_inlen;
			}
			iov->iov_base = (uintptr_t) p;
			iov->iov_len = extra_in;
			++iov;
		} else {
			command->indata = p;
		}
		command->inlen_alloc += extra_in;
	}

	return 0;
}

/*
 * Must be called once after a command involving attributes.  Figures out
 * where GET output data landed and sets .val pointers and .outlen
 * appropriately.  Returns 0 if all okay.
 */
int osd_command_attr_resolve(struct osd_command *command)
{
	struct attr_malloc_header *header = command->attr_malloc;
	uint8_t *p;
	uint64_t len;
	uint32_t list_len;
	int numget, numgetmulti, i;
	int ret = -EINVAL;
	struct attribute_list *attr = command->attr;
	int numattr = command->numattr;

	if (command->inlen < header->start_retrieved)
		goto unwind;

	p = header->retrieved_buf;
	len = command->inlen - header->start_retrieved;
	VALGRIND_MAKE_MEM_DEFINED(p, len);

	/*
	 * If getpage, just copy it in and done.  Assumes checking was
	 * done properly at the start.  Also error out the outlens for
	 * the non-getpage case to come later.
	 */
	numget = 0;
	numgetmulti = 0;
	for (i=0; i<numattr; i++) {
		if (attr[i].type == ATTR_GET_PAGE) {
			attr[i].outlen = len;
			if (attr[i].outlen > attr[i].len)
				attr[i].outlen = attr[i].len;
			attr[i].val = p;
			ret = 0;
			goto unwind;
		}
		if (attr[i].type == ATTR_GET) {
			attr[i].outlen = 0xffff;
			++numget;
	    	}
		if (attr[i].type == ATTR_GET_MULTI) {
			attr[i].val = &header->mr[numgetmulti];
			++numgetmulti;
		}
	}

	/*
	 * No ATTR_GET to process, not an error.
	 */
	if (numget == 0 && numgetmulti == 0) {
		ret = 0;
		goto unwind;
	}

	/*
	 * Read off the header first.
	 */
	if (len < 8)
		goto unwind;

	if ((p[0] & 0xf) == 0x9) {
		if (numgetmulti) {
			osd_error("%s: got list type 9, expecting multi",
				  __func__);
			goto unwind;
		}
	} else if ((p[0] & 0xf) == 0xf) {
		if (!numgetmulti) {
			osd_error("%s: got list type f, not expecting multi",
				  __func__);
			goto unwind;
		}
	} else {
		osd_error("%s: expecting list type 9 or f, got 0x%x",
			  __func__, p[0] & 0xf);
		goto unwind;
	}

	list_len = get_ntohl(&p[4]) + 8;
	if (list_len > len) {
		osd_error("%s: target says it returned %u, but really %llu",
			  __func__, list_len, llu(len));
		goto unwind;
	}
	len = list_len;
	p += 8;
	len -= 8;

	/*
	 * Process retrieved entries, figuring out where they go.
	 */
	for (;;) {
		uint64_t oid = 0;
		uint32_t page, number;
		uint16_t item_len, pad;
		uint16_t avail_len;

		if (len < 10u + 8 * !!numgetmulti)
			break;
		if (numgetmulti) {
			oid = get_ntohll(&p[0]);
			p += 8;
		}
		page = get_ntohl(&p[0]);
		number = get_ntohl(&p[4]);
		item_len = get_ntohs(&p[8]);
		p += 10;
		len -= 10;

		for (i=0; i<numattr; i++) {
			if (!(attr[i].type == ATTR_GET
			   || attr[i].type == ATTR_GET_MULTI))
				continue;
			if (attr[i].page == page && attr[i].number == number)
				break;
		}

		if (i == numattr) {
			osd_error("%s: page %x number %x not requested",
				  __func__, page, number);
			goto unwind;
		}

		avail_len = item_len;
		if (item_len == 0xffff)
			avail_len = 0;   /* not found on target */

		/*
		 * Ran off the end of the allocated buffer?
		 */
		if (avail_len > len)
			avail_len = len;

		/*
		 * This can happen for a list of gets since the target does
		 * not know the requested size of each one, just return what
		 * we can.
		 */
		if (avail_len > attr[i].len)
			avail_len = attr[i].len;

		/* set it, even if len is zero */
		if (attr[i].type == ATTR_GET_MULTI) {
			struct attribute_get_multi_results *mr
				= attr[i].val;

			mr->oid[mr->numoid] = oid;
			mr->val[mr->numoid] = avail_len ? p : NULL;
			mr->outlen[mr->numoid] = avail_len;
			++mr->numoid;
		} else {
			attr[i].val = avail_len ? p : NULL;
			attr[i].outlen = avail_len;
		}

		if (item_len == 0xffff)
			item_len = 0;

		pad = roundup8(10 + item_len) - (10 + item_len);
		if (item_len + pad >= len)
			break;

		p += item_len + pad;
		len -= item_len + pad;
	}
	ret = 0;

unwind:
	command->outdata = header->orig_outdata;
	command->outlen = header->orig_outlen;
	command->iov_outlen = header->orig_iov_outlen;
	command->indata = header->orig_indata;
	command->inlen_alloc = header->orig_inlen_alloc;
	command->iov_inlen = header->orig_iov_inlen;

	/* This is tricky.  We modified the inlen_alloc so that we
	 * could reserve extra space for the retrieved attributes.
	 * But if it was a short read, there is no way for the user
	 * to find that out without looking at the returned status,
	 * since the pad bytes papered over the short read.  Instead
	 * of fixing it up here, or in wait_response, we just put
	 * inlen back so it is no bigger than what was asked for.  But
	 * this is no guarantee that any of those bytes are valid
	 * read data!
	 */
	if (command->inlen > command->inlen_alloc)
		command->inlen = command->inlen_alloc;
	return ret;
}

void osd_command_attr_free(struct osd_command *command)
{
	free(command->attr_malloc);
}

/*
 * All attributes can be retrieved if asking for number 0xffffffff
 * of any page.  Further, if the page is 0xffffffff, a list of all user-defined
 * attributes on all pages will be returned.
 *
 * Could try to integrate this with the rest of the attr handlers, but it
 * would be very messy.  The number of returned entries is not known a priori.
 * Signaling that there are more items available than for which space was
 * requested is difficult too.  Instead we just have a specialized set of
 * all-attribute handlers, hoping that the need to combine this sort of
 * attribute request with others will not happen.
 */
int osd_command_attr_all_build(struct osd_command *command, uint32_t page)
{
	struct attr_malloc_header *header;
	uint8_t *p, *extra_out_buf, *extra_in_buf;

	/*
	 * In some of the later OSD specs, max space for a returned list
	 * is a 32-bit field.  Get a big chunk of memory for this, because
	 * if we can't list all the items, there is no way to start the list
	 * again from the middle.
	 */
	int len = 512 << 10;  /* 512k is the most BSG will let us do */

	/*
	 * Cannot use normal attr build as it caps the lengths of items at
	 * 16 bits.  We're asking for a whole bunch of items, each of which
	 * will be shorter than 16 bits, but together will exceed that.
	 */

	/* not build to handle other in or out data */
	if (command->inlen_alloc || command->outlen)
		return 1;

	p = Malloc(roundup8(sizeof(*header)) + 16 + len);
	if (!p)
		return 1;

	/* to free it later */
	command->attr_malloc = p;

	header = (void *) p;
	p += roundup8(sizeof(*header));

	extra_out_buf = p;
	p += 16;
	extra_in_buf = p;

	/*
	 * Build the getlist.
	 */
	p = extra_out_buf;
	p[0] = 0x1;
	p[1] = p[2] = p[3] = 0;
	set_htonl(&p[4], 8);
	set_htonl(&p[8], page);
	set_htonl(&p[12], 0xffffffff);

	p = extra_in_buf;
	header->retrieved_buf = p;

	/*
	 * Fill the CDB bits.  List format.
	 */
	set_htonl(&command->cdb[52], 16);  /* size getlist */
	set_htonl(&command->cdb[60], len);  /* size retrieved */

	command->outdata = extra_out_buf;
	command->outlen = 16;

	command->indata = extra_in_buf;
	command->inlen_alloc = len;

	return 0;
}

/*
 * Should have a bunch of "9 format" retrieved values.  Allocate room
 * for a new attr structure, return that.
 */
int osd_command_attr_all_resolve(struct osd_command *command)
{
	struct attr_malloc_header *header = command->attr_malloc;
	uint8_t *p;
	uint64_t len;
	uint32_t list_len;
	int ret = -EINVAL;

	p = header->retrieved_buf;
	len = command->inlen;
	VALGRIND_MAKE_MEM_DEFINED(p, len);

	/*
	 * Read off the header first.
	 */
	if (len < 8)
		goto unwind;

	if ((p[0] & 0xf) != 0x9) {
		osd_error("%s: expecting list type 9, got 0x%x",
			  __func__, p[0] & 0xf);
		goto unwind;
	}

	list_len = get_ntohl(&p[4]) + 8;
	if (list_len > len) {
		osd_error("%s: target says it returned %u, but really %llu",
			  __func__, list_len, llu(len));
		goto unwind;
	}
	len = list_len;
	p += 8;
	len -= 8;

	/*
	 * First figure out how many entries were returned.
	 */
	command->numattr = 0;
	for (;;) {
		uint16_t item_len, pad;

		if (len < 16u)  /* 10 header plus val plus roundup */
			break;
		item_len = get_ntohs(&p[8]);
		p += 10;
		len -= 10;

		if (item_len == 0xffff) {
			osd_error("%s: target returned item_len -1", __func__);
			goto unwind;
		}

		++command->numattr;

		pad = roundup8(10 + item_len) - (10 + item_len);
		if (item_len + pad >= len)
			break;

		p += item_len + pad;
		len -= item_len + pad;
	}

	/*
	 * Allocate space for them.  Okay if none.
	 */
	ret = 0;
	if (command->numattr == 0)
		goto unwind;

	command->attr = Malloc(command->numattr * sizeof(*command->attr));

	/*
	 * Now read it all, pointing their vals into the one big buf.
	 */
	p = header->retrieved_buf + 8;
	len = list_len - 8;
	command->numattr = 0;
	for (;;) {
		uint32_t page, number;
		uint16_t item_len, pad;
		uint16_t avail_len;

		if (len < 16u)
			break;
		page = get_ntohl(&p[0]);
		number = get_ntohl(&p[4]);
		item_len = get_ntohs(&p[8]);
		p += 10;
		len -= 10;
		avail_len = item_len;

		/*
		 * Ran off the end of the allocated buffer?  Just return
		 * the full entries and complain to caller.
		 */
		if (avail_len > len) {
			avail_len = len;
			ret = -E2BIG;
			break;
		}

		command->attr[command->numattr].val = p;
		command->attr[command->numattr].page = page;
		command->attr[command->numattr].number = number;
		command->attr[command->numattr].outlen = avail_len;
		++command->numattr;

		pad = roundup8(10 + item_len) - (10 + item_len);
		if (item_len + pad >= len)
			break;

		p += item_len + pad;
		len -= item_len + pad;
	}

unwind:
	command->outdata = header->orig_outdata;
	command->outlen = header->orig_outlen;
	command->iov_outlen = header->orig_iov_outlen;
	command->indata = header->orig_indata;
	command->inlen_alloc = header->orig_inlen_alloc;
	command->iov_inlen = header->orig_iov_inlen;
	if (command->inlen > command->inlen_alloc)
		command->inlen = command->inlen_alloc;
	return ret;
}

void osd_command_attr_all_free(struct osd_command *command)
{
	/* we hid the command->attr that points into attr_malloc,
	 * instead building our own at resolve time */
	if (command->numattr)
		free(command->attr);
	osd_command_attr_free(command);
}

int osd_command_list_resolve(struct osd_command *command)
{
	uint8_t *p = command->indata;
	uint64_t addl_len = get_ntohll(&p[0]);
	uint64_t cont_oid = get_ntohll(&p[8]);
	uint32_t lid = get_ntohl(&p[16]);
	int num_results = (addl_len-24)/8;
	uint64_t list[num_results];
	int i, listoid, list_attr;

	listoid = (p[23] & 0x40);
	char title[3];
	(listoid ? strcpy(title, "OID") : strcpy(title, "PID"));

	if (p[23] & 0x08)
		list_attr = 1;
	else if (p[23] & 0x04)
		list_attr = 0;

	/*
	 * For now, output the list elements.  Later, work this into
	 * the normal attr_resolve above.  Users have to specify what
	 * attributes they want to get, so we know what to expect and
	 * can fill in the results.  Just LIST has some different formats
	 * not handled by the current (messy) attr_resolve.
	 */
	if (lid || cont_oid)
		osd_debug("LID: %u CONTINUE_OID: %llu", lid, llu(cont_oid));

	if (list_attr) {
		/* List or store get/set attributes results in the
		 * command struct somehow,
		 * using a new method or perhaps attr_resolve?
		 *
		 * Also, will need to change how the data is extracted
		 * based on the value of list_attr, since each obj_descriptor
		 * isn't 8 bytes apart anymore if list_attr=1 (it depends
		 * on the number of attributes in the attr list
	 	 */
	}

	for (i=0; i < num_results; i++) {
		list[i] = get_ntohll(&p[24+8*i]);
		osd_debug("%s: %llu", title, llu(list[i]));

	}

	return 1;
}

int osd_command_list_collection_resolve(struct osd_command *command)
{
	uint8_t *p = command->indata;
	uint64_t addl_len = get_ntohll(&p[0]);
	uint64_t cont_oid = get_ntohll(&p[8]);
	uint32_t lid = get_ntohl(&p[16]);
	int num_results = (addl_len-24)/8;
	uint64_t list[num_results];
	int i, listoid, list_attr;

	listoid = (p[23] & 0x80);
	char title[3];
	(listoid ? strcpy(title, "OID") : strcpy(title, "CID"));

	if (p[23] & 0x08)
		list_attr = 1;
	else if (p[23] & 0x04)
		list_attr = 0;

	/*
	 * For now, output the list elements.  Later, work this into
	 * the normal attr_resolve above.  Users have to specify what
	 * attributes they want to get, so we know what to expect and
	 * can fill in the results.  Just LIST has some different formats
	 * not handled by the current (messy) attr_resolve.
	 */
	if (lid || cont_oid)
		osd_debug("LID: %u CONTINUE_OID: %llu", lid, llu(cont_oid));

	if (list_attr) {
		/* List or store get/set attributes results in the
		 * command struct somehow,
		 * using a new method or perhaps attr_resolve?
		 *
		 * Also, will need to change how the data is extracted
		 * based on the value of list_attr, since each obj_descriptor
		 * isn't 8 bytes apart anymore if list_attr=1 (it depends
		 * on the number of attributes in the attr list
	 	 */
	}

	for (i=0; i < num_results; i++) {
		list[i] = get_ntohll(&p[24+8*i]);
		osd_debug("%s: %llu", title, llu(list[i]));
	}

	return 1;
}
