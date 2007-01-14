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

int uosd_open(const char *dev)
{
	int interface_fd = open(dev, O_RDWR);
	return interface_fd;
}

void uosd_close(int fd)
{
	close(fd);
}

/*
 * Blocking, waits for a response.
 */
int uosd_wait_response(int fd, uint64_t *key)
{
	int ret;
	struct suo_response response;

	ret = read(fd, &response, sizeof(response));
	if (ret < 0)
		error_errno("%s: read response", __func__);
	if (ret != sizeof(response))
		error("%s: got %d bytes, expecting response %d bytes", __func__,
		      ret, sizeof(response));
	if (key)
		*key = response.key;
	return response.error;
}

static int write_cdb(int fd, 
		     const uint8_t *cdb, 
		     int cdb_len, 
		     enum data_direction dir,
		     const void *outbuf, 
		     size_t outlen, 
		     void *inbuf, 
		     size_t inlen)
{
	struct suo_req req;
	int ret;

	req.data_direction = dir;
	req.cdb_len = cdb_len;
	req.cdb_buf = (uint64_t) (uintptr_t)cdb;
	req.in_data_len = (uint32_t) inlen;
	req.in_data_buf = (uint64_t) (uintptr_t) inbuf;
	req.out_data_len = (uint32_t) outlen;
	req.out_data_buf = (uint64_t) (uintptr_t) outbuf;

	info("%s: write_cdb %02x len %d inbuf %p len %zu outbuf %p len %zu",
	     __func__, cdb[0], cdb_len, inbuf, inlen, outbuf, outlen);
	ret = write(fd, &req, sizeof(req));
	if (ret < 0)
		error_errno("%s: write suo request", __func__);
	return ret;
}

int uosd_cdb_nodata(int fd, const uint8_t *cdb, int cdb_len)
{
	return write_cdb(fd, cdb, cdb_len, DMA_NONE, NULL, 0, NULL, 0);
}

int uosd_cdb_write(int fd, const uint8_t *cdb, int cdb_len, const void *buf, size_t len)
{
	return write_cdb(fd, cdb, cdb_len, DMA_TO_DEVICE, buf, len, NULL, 0);
}

int uosd_cdb_read(int fd, const uint8_t *cdb, int cdb_len, void *buf, size_t len)
{
	return write_cdb(fd, cdb, cdb_len, DMA_FROM_DEVICE, NULL, 0, buf, len);
}

int uosd_cdb_bidir(int fd, const uint8_t *cdb, int cdb_len, const void *outbuf,
		   size_t outlen, void *inbuf, size_t inlen)
{
	error("%s: cannot do bidirectional yet", __func__);
	return write_cdb(fd, cdb, cdb_len, DMA_TO_DEVICE, outbuf, outlen,
	               inbuf, inlen);
}

