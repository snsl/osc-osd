#ifndef _USER_INTERFACE_H
#define _USER_INTERFACE_H

#define OSD_CDB_SIZE 200
#define BUFSIZE 1024

int format_osd(int fd, int cdb_len, int capacity);
int create_object(int fd, int cdb_len, uint64_t pid, uint64_t requested_oid,
			uint16_t num_user_objects);
int remove_object(int fd, int cdb_len, uint64_t pid, uint64_t requested_oid);
int write_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
			uint64_t buf_len, uint64_t offset, const char *buf);
int read_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
			uint64_t buf_len, uint64_t offset, char bufout[]);
int inquiry(int fd);
int flush_osd(int fd, int cdb_len);

#endif
