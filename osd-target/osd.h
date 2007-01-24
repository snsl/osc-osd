#ifndef __OSD_H
#define __OSD_H

#include "osd-types.h"

/* module interface */
int osd_open(const char *root, struct osd_device *osd);
int osd_close(struct osd_device *osd);

/* commands */
int osd_append(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len);
int osd_create(struct osd_device *osd, uint64_t pid, uint64_t requested_oid, uint16_t num);
int osd_create_and_write(struct osd_device *osd, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset);
int osd_create_collection(struct osd_device *osd, uint64_t pid, uint64_t requested_cid);
int osd_create_partition(struct osd_device *osd, uint64_t requested_pid);
int osd_flush(struct osd_device *osd, uint64_t pid, uint64_t oid, int flush_scope);
int osd_flush_collection(struct osd_device *osd, uint64_t pid, uint64_t cid,
                         int flush_scope);
int osd_flush_osd(struct osd_device *osd, int flush_scope);
int osd_flush_partition(struct osd_device *osd, uint64_t pid, int flush_scope);
int osd_format_osd(struct osd_device *osd, uint64_t capacity);
int osd_get_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid);
int osd_get_member_attributes(struct osd_device *osd, uint64_t pid, uint64_t cid);
int osd_list(struct osd_device *osd, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid);
int osd_list_collection(struct osd_device *osd, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid);
int osd_query(struct osd_device *osd, uint64_t pid, uint64_t cid, uint32_t query_len,
              uint64_t alloc_len);
int osd_read(struct osd_device *osd, uint64_t pid, uint64_t uid, uint64_t len,
	     uint64_t offset);
int osd_remove(struct osd_device *osd, uint64_t pid, uint64_t uid);
int osd_remove_collection(struct osd_device *osd, uint64_t pid, uint64_t cid);
int osd_remove_member_objects(struct osd_device *osd, uint64_t pid, uint64_t cid);
int osd_remove_partition(struct osd_device *osd, uint64_t pid);
int osd_set_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid);
int osd_set_key(struct osd_device *osd, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20]);
int osd_set_master_key(struct osd_device *osd, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len);
int osd_set_member_attributes(struct osd_device *osd, uint64_t pid, uint64_t cid);
int osd_write(struct osd_device *osd, uint64_t pid, uint64_t uid, uint64_t len,
	      uint64_t offset);

#endif /* __OSD_H */
