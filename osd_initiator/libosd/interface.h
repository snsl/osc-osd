
#include <stdint.h>
#include <sys/types.h>

enum data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

int interface_init(const char *dev);
void interface_exit(void);
int cdb_nodata_cmd(const uint8_t *cdb, int cdb_len);
int cdb_write_cmd(const uint8_t *cdb, int cdb_len, const void *buf, size_t len);
int cdb_read_cmd(const uint8_t *cdb, int cdb_len, void *buf, size_t len);
int cdb_bidir_cmd(const uint8_t *cdb, int cdb_len, const void *outbuf,
                  size_t outlen, void *inbuf, size_t inlen);

