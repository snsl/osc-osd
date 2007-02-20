#ifndef _USER_INTERFACE_SGIO_H
#define _USER_INTERFACE_SGIO_H

int inquiry_sgio(int fd);
int create_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects);
int remove_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid);
int write_osd_sgio(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset);
int read_osd_sgio(int fd, uint64_t pid, uint64_t oid, uint64_t offset);
int format_osd_sgio(int fd, int capacity);
int flush_osd_sgio(int fd, int flush_scope);


#endif
