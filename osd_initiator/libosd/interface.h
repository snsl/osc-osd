#include <stdint.h>
#include <sys/types.h>
#include "common.h"

enum data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

struct dev_response {
	uint64_t key;
	int error;
	int sense_buffer_len;
	unsigned char sense_buffer[252];  /* hackety hack */
};


/*
 * Public functions
 */

int dev_osd_open(const char *dev);
void dev_osd_close(int fd);
int dev_osd_wait_response(int fd, uint64_t *key);
int dev_osd_wait_response2(int fd, struct dev_response *devresp);
int dev_osd_write_nodata(int fd, const uint8_t *cdb, int cdb_len);
int dev_osd_write(int fd, const uint8_t *cdb, int cdb_len, const void *buf, size_t len);
int dev_osd_read(int fd, const uint8_t *cdb, int cdb_len, void *buf, size_t len);
int dev_osd_bidir(int fd, const uint8_t *cdb, int cdb_len, const void *outbuf,
		   size_t outlen, void *inbuf, size_t inlen);
void hexdump(uint8_t *d, size_t len);
void dev_show_sense(uint8_t *sense, int len);

/*Functions to set up CDB*/
int set_cdb_osd_append(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len);
int set_cdb_osd_create(uint8_t *cdb, uint64_t pid, uint64_t requested_oid,
               uint16_t num);
int set_cdb_osd_create_and_write(uint8_t *cdb, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset);
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
int set_cdb_osd_list(uint8_t *cdb, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid);
int set_cdb_osd_list_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid);
int set_cdb_osd_perform_scsi_command(uint8_t *cdb);
int set_cdb_osd_perform_task_mgmt_func(uint8_t *cdb);
int set_cdb_osd_query(uint8_t *cdb, uint64_t pid, uint64_t cid, uint32_t query_len,
              uint64_t alloc_len);
int set_cdb_osd_read(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
	     uint64_t offset);
int set_cdb_osd_remove(uint8_t *cdb, uint64_t pid, uint64_t oid);
int set_cdb_osd_remove_collection(uint8_t *cdb, uint64_t pid, uint64_t cid);
int set_cdb_osd_remove_member_objects(uint8_t *cdb, uint64_t pid, uint64_t cid);
int set_cdb_osd_remove_partition(uint8_t *cdb, uint64_t pid);
int set_cdb_osd_set_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid);
int set_cdb_osd_set_key(uint8_t *cdb, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20]);
int set_cdb_osd_set_master_key(uint8_t *cdb, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len);
int set_cdb_osd_set_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid);
int set_cdb_osd_write(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
	      uint64_t offset);
