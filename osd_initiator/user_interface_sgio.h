#ifndef _USER_INTERFACE_SGIO_H
#define _USER_INTERFACE_SGIO_H

struct attribute_id {
        uint32_t page;
        uint32_t number;
        uint16_t len;
};

/* Queries */
int inquiry_sgio(int fd);
int query_sgio(int fd, uint64_t pid, uint64_t cid, const char *query);
/* Create */
int create_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects);
int create_partition_sgio(int fd, uint64_t requested_pid);
int create_collection_sgio(int fd, uint64_t pid, uint64_t requested_cid);
int create_osd_and_write_sgio(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset);
/* Read/Write */
int write_osd_sgio(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset);
int read_osd_sgio(int fd, uint64_t pid, uint64_t oid, uint64_t offset);
/* Remove */
int remove_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid);
int remove_partition_sgio(int fd, uint64_t pid);
int remove_collection_sgio(int fd, uint64_t pid, uint64_t cid);
int remove_member_objects_sgio(int fd, uint64_t pid, uint64_t cid);
/* Format/Flush */
int format_osd_sgio(int fd, int capacity);
int flush_osd_sgio(int fd, int flush_scope);
int flush_partition_sgio(int fd, uint64_t pid, int flush_scope);
int flush_collection_sgio(int fd, uint64_t pid, uint64_t cid, int flush_scope);
/* Get/Set Attributes */
int get_attributes_sgio(int fd, uint64_t pid, uint64_t oid);
int get_member_attributes_sgio(int fd, uint64_t pid, uint64_t cid);
int get_attribute_page_sgio(int fd, uint64_t page, uint64_t offset);
int set_attributes_sgio(int fd, uint64_t pid, uint64_t oid, const struct attribute_id *attrs);
int set_member_attributes_sgio(int fd, uint64_t pid, uint64_t cid, const struct attribute_id *attrs);
/* List */
int object_list_sgio(int fd, uint64_t pid, uint32_t list_id, uint64_t initial_oid);
int collection_list_sgio(int fd, uint64_t pid, uint64_t cid, uint32_t list_id, uint64_t initial_oid);

#endif
