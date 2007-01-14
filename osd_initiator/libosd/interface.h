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
