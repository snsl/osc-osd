#include <stdint.h>
#include <sys/types.h>
#include "common.h"

enum data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};


/*
 * Public functions
 */

int uosd_open(const char *dev);
void uosd_close(int fd);
int uosd_wait_response(int fd, uint64_t *key);
int uosd_cdb_nodata(int fd, const uint8_t *cdb, int cdb_len);
int uosd_cdb_write(int fd, const uint8_t *cdb, int cdb_len, const void *buf, size_t len);
int uosd_cdb_read(int fd, const uint8_t *cdb, int cdb_len, void *buf, size_t len);
int uosd_cdb_bidir(int fd, const uint8_t *cdb, int cdb_len, const void *outbuf,
		   size_t outlen, void *inbuf, size_t inlen);
int osd_append(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len);
int osd_create(uint8_t *cdb, uint64_t pid, uint64_t requested_oid,
               uint16_t num);
int osd_create_and_write(uint8_t *cdb, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset);
int osd_create_collection(uint8_t *cdb, uint64_t pid, uint64_t requested_cid);
int osd_create_partition(uint8_t *cdb, uint64_t requested_pid);
int osd_flush(uint8_t *cdb, uint64_t pid, uint64_t oid, int flush_scope);
int osd_flush_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,
                         int flush_scope);
int osd_flush_osd(uint8_t *cdb, int flush_scope);
int osd_flush_partition(uint8_t *cdb, uint64_t pid, int flush_scope);
int osd_format_osd(uint8_t *cdb, uint64_t capacity);
int osd_get_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid);
int osd_get_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid);
int osd_list(uint8_t *cdb, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid);
int osd_list_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid);
int osd_perform_scsi_command(uint8_t *cdb);
int osd_perform_task_mgmt_func(uint8_t *cdb);
int osd_query(uint8_t *cdb, uint64_t pid, uint64_t cid, uint32_t query_len,
              uint64_t alloc_len);
int osd_read(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
	     uint64_t offset);
int osd_remove(uint8_t *cdb, uint64_t pid, uint64_t oid);
int osd_remove_collection(uint8_t *cdb, uint64_t pid, uint64_t cid);
int osd_remove_member_objects(uint8_t *cdb, uint64_t pid, uint64_t cid);
int osd_remove_partition(uint8_t *cdb, uint64_t pid);
int osd_set_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid);
int osd_set_key(uint8_t *cdb, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20]);
int osd_set_master_key(uint8_t *cdb, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len);
int osd_set_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid);
int osd_write(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
	      uint64_t offset);
