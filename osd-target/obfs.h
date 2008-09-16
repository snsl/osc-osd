#ifndef __OBFS_H
#define __OBFS_H

#include "obfs-types.h"

/* module interface */
int osd_open(const char *root, osd_t *osd);
int osd_close(osd_t *osd);

/* commands */
int osd_append(osd_t *osd, uint64_t pid, uint64_t oid, uint64_t len);
int osd_create(osd_t *osd, uint64_t pid, uint64_t requested_oid, uint16_t num);
int osd_create_and_write(osd_t *osd, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset);
int osd_create_collection(osd_t *osd, uint64_t pid, uint64_t requested_cid);
int osd_create_partition(osd_t *osd, uint64_t requested_pid);
int osd_flush(osd_t *osd, uint64_t pid, uint64_t oid, int flush_scope);
int osd_flush_collection(osd_t *osd, uint64_t pid, uint64_t cid,
                         int flush_scope);
int osd_flush_osd(osd_t *osd, int flush_scope);
int osd_flush_partition(osd_t *osd, uint64_t pid, int flush_scope);
int osd_format(osd_t *osd, uint64_t capacity);
int osd_get_attributes(osd_t *osd, uint64_t pid, uint64_t oid);
int osd_get_member_attributes(osd_t *osd, uint64_t pid, uint64_t cid);
int osd_list(osd_t *osd, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid);
int osd_list_collection(osd_t *osd, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid);
int osd_query(osd_t *osd, uint64_t pid, uint64_t cid, uint32_t query_len,
              uint64_t alloc_len);
int osd_read(osd_t *osd, uint64_t pid, uint64_t uid, uint64_t len,
	     uint64_t offset);
int osd_remove(osd_t *osd, uint64_t pid, uint64_t uid);
int osd_remove_collection(osd_t *osd, uint64_t pid, uint64_t cid);
int osd_remove_member_objects(osd_t *osd, uint64_t pid, uint64_t cid);
int osd_remove_partition(osd_t *osd, uint64_t pid);
int osd_set_attributes(osd_t *osd, uint64_t pid, uint64_t oid);
int osd_set_key(osd_t *osd, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20]);
int osd_set_master_key(osd_t *osd, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len);
int osd_set_member_attributes(osd_t *osd, uint64_t pid, uint64_t cid);
int osd_write(osd_t *osd, uint64_t pid, uint64_t uid, uint64_t len,
	      uint64_t offset);

#endif /* __OBFS_H */
