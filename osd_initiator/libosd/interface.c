/*
 * Talk to the kernel module.
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"
#include "interface.h"
#include "../kernel/suo_ioctl.h"

static int interface_fd = -1;

/*
 * Open the chardev
 */
int interface_init(const char *dev)
{
	interface_fd = open(dev, O_RDWR);
	if (interface_fd < 0)
		error_errno("%s: open %s", __func__, dev);
	return 0;
}

void interface_exit(void)
{
	close(interface_fd);
}

static int cdb_cmd(const uint8_t *cdb, int cdb_len, enum data_direction dir,
                   const void *outbuf, size_t outlen,
                   void *inbuf, size_t inlen)
{
	struct suo_req req;
	int ret;

	req.data_direction = dir;
	req.cdb_len = cdb_len;
	req.cdb_buf = (uint64_t) (uintptr_t) cdb;
	req.in_data_len = (uint32_t) inlen;
	req.in_data_buf = (uint64_t) (uintptr_t) inbuf;
	req.out_data_len = (uint32_t) outlen;
	req.out_data_buf = (uint64_t) (uintptr_t) outbuf;

	info("%s: cdb %02x len %d inbuf %p len %zu outbuf %p len %zu",
	     __func__, cdb[0], cdb_len, inbuf, inlen, outbuf, outlen);
	ret = write(interface_fd, &req, sizeof(req));
	if (ret < 0)
		error_errno("%s: write suo request", __func__);
	return ret;
}

int cdb_nodata_cmd(const uint8_t *cdb, int cdb_len)
{
	return cdb_cmd(cdb, cdb_len, DMA_NONE, NULL, 0, NULL, 0);
}

int cdb_write_cmd(const uint8_t *cdb, int cdb_len, const void *buf, size_t len)
{
	return cdb_cmd(cdb, cdb_len, DMA_TO_DEVICE, buf, len, NULL, 0);
}

int cdb_read_cmd(const uint8_t *cdb, int cdb_len, void *buf, size_t len)
{
	return cdb_cmd(cdb, cdb_len, DMA_FROM_DEVICE, NULL, 0, buf, len);
}

int cdb_bidir_cmd(const uint8_t *cdb, int cdb_len, const void *outbuf,
                  size_t outlen, void *inbuf, size_t inlen)
{
	error("%s: cannot do bidirectional yet", __func__);
	return cdb_cmd(cdb, cdb_len, DMA_TO_DEVICE, outbuf, outlen,
	               inbuf, inlen);
}

