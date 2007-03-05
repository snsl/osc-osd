#ifndef _SYNC_H
#define _SYNC_H

/*
 * These functions do everything:  build, submit, wait response.
 */

/* Queries */
int inquiry(int fd);
int query(int fd, uint64_t pid, uint64_t cid, const char *query);
/* Create */
int create_osd(int fd, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects);
int create_partition(int fd, uint64_t requested_pid);
int create_collection(int fd, uint64_t pid, uint64_t requested_cid);
int create_osd_and_write(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset);
/* Read/Write */
int write_osd(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset);
int read_osd(int fd, uint64_t pid, uint64_t oid, uint64_t offset);
/* Remove */
int remove_osd(int fd, uint64_t pid, uint64_t requested_oid);
int remove_partition(int fd, uint64_t pid);
int remove_collection(int fd, uint64_t pid, uint64_t cid);
int remove_member_objects(int fd, uint64_t pid, uint64_t cid);
/* Format/Flush */
int format_osd(int fd, int capacity);
int flush_osd(int fd, int flush_scope);
int flush_partition(int fd, uint64_t pid, int flush_scope);
int flush_collection(int fd, uint64_t pid, uint64_t cid, int flush_scope);
/* Get/Set Attributes */
int get_attributes(int fd, uint64_t pid, uint64_t oid);
int get_member_attributes(int fd, uint64_t pid, uint64_t cid);
int set_attributes(int fd, uint64_t pid, uint64_t oid, const struct attribute_id *attrs);
int set_member_attributes(int fd, uint64_t pid, uint64_t cid, const struct attribute_id *attrs);
/* List */
int object_list(int fd, uint64_t pid, uint32_t list_id, uint64_t initial_oid);
int collection_list(int fd, uint64_t pid, uint64_t cid, uint32_t list_id, uint64_t initial_oid);

int osd_command_attr_build(struct osd_command *command,
                           struct attribute_id *attrs, int num);
uint8_t *osd_command_attr_resolve(struct osd_command *command,
                                  struct attribute_id *attrs, int num,
			          int index);

#endif
