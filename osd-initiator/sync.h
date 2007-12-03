#ifndef _SYNC_H
#define _SYNC_H

/*
 * These functions do everything:  build, submit, wait response.
 */

/* Queries */
int inquiry(int fd);
int query(int fd, uint64_t pid, uint64_t cid, const uint8_t *query);
/* Create */
int create_osd(int fd, uint64_t pid, uint64_t requested_oid,
	       uint16_t num_user_objects);
int create_partition(int fd, uint64_t requested_pid);
int create_collection(int fd, uint64_t pid, uint64_t requested_cid);
int create_osd_and_write(int fd, uint64_t pid, uint64_t requested_oid,
			 const uint8_t *buf, uint64_t len, uint64_t offset);
/* Read/Write */
int write_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	      uint64_t len, uint64_t offset);
int write_sgl_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	      uint64_t len, uint64_t offset);
int read_osd(int fd, uint64_t pid, uint64_t oid, uint8_t *buf, uint64_t len,
	     uint64_t offset);
int append_osd(int fd, uint64_t pid, uint64_t oid, const uint8_t *buf,
	       uint64_t len);

/* Remove */
int remove_osd(int fd, uint64_t pid, uint64_t requested_oid);
int remove_partition(int fd, uint64_t pid);
int remove_collection(int fd, uint64_t pid, uint64_t cid, int force);
int remove_member_objects(int fd, uint64_t pid, uint64_t cid);
/* Format/Flush */
int format_osd(int fd, int capacity);
int flush_osd(int fd, int flush_scope);
int flush_partition(int fd, uint64_t pid, int flush_scope);
int flush_collection(int fd, uint64_t pid, uint64_t cid, int flush_scope);
int flush_object(int fd, uint64_t pid, uint64_t oid, int flush_scope);
/*
 * Get/Set Attributes - none of these work at the moment, use attr_build
 * and attr_resolve in command.c
 */
int get_attributes(int fd, uint64_t pid, uint64_t oid);
int get_member_attributes(int fd, uint64_t pid, uint64_t cid);
int set_attributes(int fd, uint64_t pid, uint64_t oid,
		   const struct attribute_list *attrs);
int set_member_attributes(int fd, uint64_t pid, uint64_t cid,
			  const struct attribute_list *attrs);
/* List */
int list(int fd, uint64_t pid, uint32_t list_id, uint64_t initial_oid,
	 uint64_t alloc_len, int list_attr);
int list_collection(int fd, uint64_t pid, uint64_t cid, uint32_t list_id,
		    uint64_t initial_oid, uint64_t alloc_len, int list_attr);

#endif
