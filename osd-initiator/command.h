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
#ifndef _COMMAND_H
#define _COMMAND_H

#include <stdint.h>
#include "osd-util/osd-defs.h"

/* Data Structure Definition */

struct attribute_list {
	enum {
		ATTR_GET,
		ATTR_GET_PAGE,
		ATTR_GET_MULTI,
		ATTR_SET,
		ATTR_RESULT,
	} type;
	uint32_t page;
	uint32_t number;
	void *val;
	uint16_t len;
	uint16_t outlen;  /* 0 -> empty or not exist, 0xffff -> overflow */
};

/*
 * In the case of ATTR_GET_MULTI, each returned attr->val will point to
 * one of these.  Look inside it to find the values for each oid.
 */
struct attribute_get_multi_results {
	int numoid;
	union {
		uint64_t *oid;
		uint64_t *cid;
		uint64_t *pid;
	};
	const void **val;  /* arrays for each oid:  val, outlen */
	uint16_t *outlen;
};

/* The Linux kernel does not currently support BSG iovecs.  The
   bsg_iovec struct below assumes a kernel patch that has not been
   incorporated into the kernel.  Therefore, it is removed for now,
   and just set to point to the scsi/sg.h sg_iovec (which is ignored
   in the kernel
*/
#ifdef __APPLE__
/*
 * This is copied from a kernel header to avoid including it.
 */
struct bsg_iovec {
	uint64_t iov_base;
	uint32_t iov_len;
	uint32_t __pad1;
};
#else
#include <scsi/sg.h>
#define bsg_iovec sg_iovec
#endif

/*
 * All information needed to submit a command to the kernel module.
 *   [i] = provided by caller to submit_osd()
 *   [o] = return from library to caller
 */
struct osd_command {
	uint8_t cdb[OSD_CDB_SIZE];	/* [i] maximum length CDB */
	int cdb_len;			/* [i] actual len of valid bytes */
	const void *outdata;		/* [i] data, goes out to target */
	size_t outlen;			/* [i] length */
	int iov_outlen;			/* [i] if non-zero, data are iovecs */
	void *indata;			/* [o] results, returned from target */
	size_t inlen_alloc;		/* [i] alloc size for results */
	size_t inlen;			/* [o] actual size returned */
	int iov_inlen;			/* [i] */
	uint8_t status;			/* [o] scsi status */
	uint8_t sense[OSD_MAX_SENSE];	/* [o] sense errors */
	int sense_len;			/* [o] number of bytes in sense */
	struct attribute_list *attr;    /* [o] after attr_resolve() */
	int numattr;                    /* [o] */
	void *attr_malloc;		/* [x] internal use only */
};


/* Set up the CDB */
int osd_command_set_test_unit_ready(struct osd_command *command);
int osd_command_set_inquiry(struct osd_command *command, uint8_t page_code,
			    uint8_t outlen);
int osd_command_set_append(struct osd_command *command, uint64_t pid,
			   uint64_t oid, uint64_t len);
int osd_command_set_clear(struct osd_command *command, uint64_t pid,
			  uint64_t oid, uint64_t len, uint64_t offset);
int osd_command_set_create(struct osd_command *command, uint64_t pid,
			   uint64_t requested_oid, uint16_t num_user_objects);
int osd_command_set_create_and_write(struct osd_command *command, uint64_t pid,
				     uint64_t requested_oid, uint64_t len,
				     uint64_t offset);
int osd_command_set_create_collection(struct osd_command *command,
				      uint64_t pid, uint64_t requested_cid);
int osd_command_set_create_partition(struct osd_command *command,
				     uint64_t requested_pid);
int osd_command_set_create_user_tracking_collection(struct osd_command *command,
						    uint64_t pid,
						    uint64_t requested_cid,
						    uint64_t source_cid);
int osd_command_set_flush(struct osd_command *command, uint64_t pid, uint64_t len,
			  uint64_t offset, uint64_t oid, int flush_scope);
int osd_command_set_flush_collection(struct osd_command *command, uint64_t pid,
				     uint64_t cid, int flush_scope);
int osd_command_set_flush_osd(struct osd_command *command, int flush_scope);
int osd_command_set_flush_partition(struct osd_command *command, uint64_t pid,
				    int flush_scope);
int osd_command_set_format_osd(struct osd_command *command, uint64_t capacity);
int osd_command_set_get_attributes(struct osd_command *command, uint64_t pid,
				   uint64_t oid);
int osd_command_set_get_member_attributes(struct osd_command *command,
					  uint64_t pid, uint64_t cid);
int osd_command_set_list(struct osd_command *command, uint64_t pid,
			 uint32_t list_id, uint64_t alloc_len,
			 uint64_t initial_oid, int list_attr);
int osd_command_set_list_collection(struct osd_command *command, uint64_t pid,
				    uint64_t cid, uint32_t list_id,
				    uint64_t alloc_len,
				    uint64_t initial_oid,
				    int list_attr);
int osd_command_set_perform_scsi_command(struct osd_command *command);
int osd_command_set_perform_task_mgmt_func(struct osd_command *command);
int osd_command_set_punch(struct osd_command *command, uint64_t pid,
			  uint64_t oid, uint64_t len, uint64_t offset);
int osd_command_set_query(struct osd_command *command, uint64_t pid,
			  uint64_t cid, uint32_t cont_len, uint64_t alloc_len,
			  uint64_t matches_cid);
int osd_command_set_read(struct osd_command *command, uint64_t pid,
			 uint64_t oid, uint64_t len, uint64_t offset);
int osd_command_set_read_map(struct osd_command*command, uint64_t pid, uint64_t oid,
			     uint64_t alloc_len, uint64_t offset, uint8_t map_type);
int osd_command_set_remove(struct osd_command *command, uint64_t pid,
			   uint64_t oid);
int osd_command_set_remove_collection(struct osd_command *command,
				      uint64_t pid, uint64_t cid, int force);
int osd_command_set_remove_member_objects(struct osd_command *command,
					  uint64_t pid, uint64_t cid);
int osd_command_set_remove_partition(struct osd_command *command,
				     uint64_t pid);
int osd_command_set_set_attributes(struct osd_command *command, uint64_t pid,
				   uint64_t oid);
int osd_command_set_set_key(struct osd_command *command, int key_to_set,
			    uint64_t pid, uint64_t key, const uint8_t seed[20]);
int osd_command_set_set_master_key(struct osd_command *command, int dh_step,
				   uint64_t key, uint32_t param_len,
				   uint32_t alloc_len);
int osd_command_set_set_member_attributes(struct osd_command *command,
					  uint64_t pid, uint64_t cid);
int osd_command_set_write(struct osd_command *command, uint64_t pid,
			  uint64_t oid, uint64_t len, uint64_t offset);
void osd_command_set_ddt(struct osd_command *command, uint8_t ddt_type);
uint8_t osd_command_get_ddt(struct osd_command *command);
/*
 * Extensions, not yet in T10 or SNIA OSD spec.
 */
int osd_command_set_cas(struct osd_command *command, uint64_t pid,
			uint64_t oid, uint64_t len, uint64_t offset);
int osd_command_set_fa(struct osd_command *command, uint64_t pid,
		       uint64_t oid, uint64_t len, uint64_t offset);
int osd_command_set_gen_cas(struct osd_command *command, uint64_t pid,
			    uint64_t oid);
int osd_command_set_cond_setattr(struct osd_command *command, uint64_t pid,
				 uint64_t oid);

/* Attributes */
int osd_command_attr_build(struct osd_command *command,
			   const struct attribute_list *const attrs, int num);
int osd_command_attr_resolve(struct osd_command *command);
void osd_command_attr_free(struct osd_command *command);

/* Get all attributes */
int osd_command_attr_all_build(struct osd_command *command, uint32_t page);
int osd_command_attr_all_resolve(struct osd_command *command);
void osd_command_attr_all_free(struct osd_command *command);

/* Lists */
int osd_command_list_resolve(struct osd_command *command);
int osd_command_list_collection_resolve(struct osd_command *command);

#endif
