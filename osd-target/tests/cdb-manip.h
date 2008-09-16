#ifndef _CDB_MANIP_H
#define _CDB_MANIP_H

#include "osd.h"

void cdb_build_inquiry(uint8_t *cdb, uint8_t outlen);
void varlen_cdb_init(uint8_t *cdb);
void set_action(uint8_t *cdb, uint16_t command);

/* CDB Manipulation functions */
int set_cdb_osd_append(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len);
int set_cdb_osd_create(uint8_t *cdb, uint64_t pid, uint64_t requested_oid,
		       uint16_t num);
int set_cdb_osd_create_and_write(uint8_t *cdb, uint64_t pid, 
				 uint64_t requested_oid, uint64_t len,
				 uint64_t offset);
int set_cdb_osd_create_collection(uint8_t *cdb, uint64_t pid, uint64_t requested_cid);
int set_cdb_osd_create_partition(uint8_t *cdb, uint64_t requested_pid);
int set_cdb_osd_flush(uint8_t *cdb, uint64_t pid, uint64_t oid, int flush_scope);
int set_cdb_osd_flush_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,
				 int flush_scope);
int set_cdb_osd_flush_osd(uint8_t *cdb, int flush_scope);
int set_cdb_osd_flush_partition(uint8_t *cdb, uint64_t pid, int flush_scope);
int set_cdb_osd_format_osd(uint8_t *cdb, uint64_t capacity);
int set_cdb_osd_get_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid);
int set_cdb_osd_get_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid);
int set_cdb_osd_list(uint8_t *cdb, uint64_t pid, uint32_t list_id, 
		     uint64_t alloc_len, uint64_t initial_oid);
int set_cdb_osd_list_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,
				uint32_t list_id, uint64_t alloc_len,
				uint64_t initial_oid);
int set_cdb_osd_perform_scsi_command(uint8_t *cdb);
int set_cdb_osd_perform_task_mgmt_func(uint8_t *cdb);
int set_cdb_osd_query(uint8_t *cdb, uint64_t pid, uint64_t cid, 
		      uint32_t query_len, uint64_t alloc_len);
int set_cdb_osd_read(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
		     uint64_t offset);
int set_cdb_osd_remove(uint8_t *cdb, uint64_t pid, uint64_t oid);
int set_cdb_osd_remove_collection(uint8_t *cdb, uint64_t pid, uint64_t cid);
int set_cdb_osd_remove_member_objects(uint8_t *cdb, uint64_t pid, uint64_t cid);
int set_cdb_osd_remove_partition(uint8_t *cdb, uint64_t pid);
int set_cdb_osd_set_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid);
int set_cdb_osd_set_key(uint8_t *cdb, int key_to_set, uint64_t pid, 
			uint64_t key, uint8_t seed[20]);
int set_cdb_osd_set_master_key(uint8_t *cdb, int dh_step, uint64_t key,
			       uint32_t param_len, uint32_t alloc_len);
int set_cdb_osd_set_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid);
int set_cdb_osd_write(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
		      uint64_t offset);
int set_cdb_getpage_setvalue(uint8_t *cdb, uint32_t getpage, 
			     uint32_t alloclen, uint32_t retrieved_offset,
			     uint32_t setpage, uint32_t setnumber,
			     uint32_t setlen, uint32_t setattr_offset);
int set_cdb_getlist_setlist(uint8_t *cdb, uint32_t getlist_len,
			    uint32_t getlist_offset, uint32_t alloc_len,
			    uint32_t retrieved_offset, uint32_t setlist_len,
			    uint32_t setlist_offset);
int set_cdb_setattr_list(void *buf, struct list_entry *le, uint32_t numle);
int set_cdb_getattr_list(void *buf, struct getattr_list_entry *gl, 
			 uint32_t numgl);
#endif
