#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <getopt.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>

#include "util/osd-util.h"
#include "util/bsg.h"

/*
 * Be sure to pick the disk correctly.  Write test will destroy it.
 */
#define BLOCK_DEV "/dev/sdb"
#define BSG_DEV "/dev/bsg/sdb"

static double *b;
static void *buf;

static void block_write_bw(int iters, int sz)
{
	int i, ret, fd;
	uint64_t start, end;

	fd = open(BLOCK_DEV, O_WRONLY | O_DIRECT);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, BLOCK_DEV);

	/* warm up */
	for (i=0; i<5; i++) {
		ret = pwrite(fd, buf, sz, 0);
		assert(ret == sz);
	}

	for (i=0; i<iters; i++) {
		rdtsc(start);
		ret = pwrite(fd, buf, sz, 0);
		rdtsc(end);
		assert(ret == sz);
		b[i] = (double) (end - start);
	}

	close(fd);
}

static void block_read_bw(int iters, int sz)
{
	int i, ret, fd;
	uint64_t start, end;

	fd = open(BLOCK_DEV, O_RDONLY | O_DIRECT);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, BLOCK_DEV);

	/* warm up */
	for (i=0; i<5; i++) {
		ret = pread(fd, buf, sz, 0);
		assert(ret == sz);
	}

	for (i=0; i<iters; i++) {
		rdtsc(start);
		ret = pread(fd, buf, sz, 0);
		rdtsc(end);
		assert(ret == sz);
		b[i] = (double) (end - start);
	}

	close(fd);
}

static void sg_write_bw(int iters, int sz)
{
	int i, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_hdr sg;

	fd = open(BLOCK_DEV, O_RDWR | O_DIRECT | O_NONBLOCK);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, BLOCK_DEV);

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x2a;  /* WRITE_10 */
	assert(sz <= 65535 * 512);
	assert((sz & 511) == 0);
	blocks = sz / 512;
	cdb[7] = (blocks >> 8) & 0xff;
	cdb[8] = blocks & 0xff;

	memset(&sg, 0, sizeof(sg));
	sg.interface_id = 'S';
	sg.cmd_len = sizeof(cdb);
	sg.cmdp = cdb;
	sg.dxfer_direction = SG_DXFER_TO_DEV;
	sg.dxfer_len = sz;
	sg.dxferp = buf;
	sg.timeout = 30000;  /* 30 sec */

	/* warm up */
	for (i=0; i<5; i++) {
		ret = ioctl(fd, SG_IO, &sg);
		assert(ret == 0);
		assert((sg.status & 0x7e) == 0 && sg.host_status == 0 &&
		       (sg.driver_status & 0xf) == 0);
	}

	for (i=0; i<iters; i++) {
		rdtsc(start);
		ret = ioctl(fd, SG_IO, &sg);
		rdtsc(end);
		assert(ret == 0);
		assert((sg.status & 0x7e) == 0 && sg.host_status == 0 &&
		       (sg.driver_status & 0xf) == 0);
		b[i] = (double) (end - start);
	}

	close(fd);
}

static void sg_read_bw(int iters, int sz)
{
	int i, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_hdr sg;

	fd = open(BLOCK_DEV, O_RDWR | O_DIRECT | O_NONBLOCK);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, BLOCK_DEV);

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x28;  /* READ_10 */
	assert(sz <= 65535 * 512);
	assert((sz & 511) == 0);
	blocks = sz / 512;
	cdb[7] = (blocks >> 8) & 0xff;
	cdb[8] = blocks & 0xff;

	memset(&sg, 0, sizeof(sg));
	sg.interface_id = 'S';
	sg.cmd_len = sizeof(cdb);
	sg.cmdp = cdb;
	sg.dxfer_direction = SG_DXFER_FROM_DEV;
	sg.dxfer_len = sz;
	sg.dxferp = buf;
	sg.timeout = 30000;  /* 30 sec */

	/* warm up */
	for (i=0; i<5; i++) {
		ret = ioctl(fd, SG_IO, &sg);
		assert(ret == 0);
		assert((sg.status & 0x7e) == 0 && sg.host_status == 0 &&
		       (sg.driver_status & 0xf) == 0);
	}

	for (i=0; i<iters; i++) {
		rdtsc(start);
		ret = ioctl(fd, SG_IO, &sg);
		rdtsc(end);
		assert(ret == 0);
		assert((sg.status & 0x7e) == 0 && sg.host_status == 0 &&
		       (sg.driver_status & 0xf) == 0);
		b[i] = (double) (end - start);
	}

	close(fd);
}

static void bsg_write_bw(int iters, int sz)
{
	int i, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_v4 bsg;

	fd = open(BSG_DEV, O_RDWR);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, BSG_DEV);

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x2a;  /* WRITE_10 */
	assert(sz <= 65535 * 512);
	assert((sz & 511) == 0);
	blocks = sz / 512;
	cdb[7] = (blocks >> 8) & 0xff;
	cdb[8] = blocks & 0xff;

	memset(&bsg, 0, sizeof(bsg));
	bsg.guard = 'Q';
	bsg.request_len = sizeof(cdb);
	bsg.request = (uint64_t) (uintptr_t) cdb;
	bsg.dout_xfer_len = sz;
	bsg.dout_xferp = (uint64_t) (uintptr_t) buf;
	bsg.timeout = 30000;  /* 30 sec */

	/* warm up */
	for (i=0; i<5; i++) {
		ret = ioctl(fd, SG_IO, &bsg);
		assert(ret == 0);
		assert(bsg.device_status == 0);
	}

	for (i=0; i<iters; i++) {
		rdtsc(start);
		ret = ioctl(fd, SG_IO, &bsg);
		rdtsc(end);
		assert(ret == 0);
		assert(bsg.device_status == 0);
		b[i] = (double) (end - start);
	}

	close(fd);
}

static void bsg_read_bw(int iters, int sz)
{
	int i, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_v4 bsg;

	fd = open(BSG_DEV, O_RDWR);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, BSG_DEV);

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x28;  /* READ_10 */
	assert(sz <= 65535 * 512);
	assert((sz & 511) == 0);
	blocks = sz / 512;
	cdb[7] = (blocks >> 8) & 0xff;
	cdb[8] = blocks & 0xff;

	memset(&bsg, 0, sizeof(bsg));
	bsg.guard = 'Q';
	bsg.request_len = sizeof(cdb);
	bsg.request = (uint64_t) (uintptr_t) cdb;
	bsg.din_xfer_len = sz;
	bsg.din_xferp = (uint64_t) (uintptr_t) buf;
	bsg.timeout = 30000;  /* 30 sec */

	/* warm up */
	for (i=0; i<5; i++) {
		ret = ioctl(fd, SG_IO, &bsg);
		assert(ret == 0);
		assert(bsg.device_status == 0 && bsg.din_resid == 0);
	}

	for (i=0; i<iters; i++) {
		rdtsc(start);
		ret = ioctl(fd, SG_IO, &bsg);
		rdtsc(end);
		assert(ret == 0);
		assert(bsg.device_status == 0 && bsg.din_resid == 0);
		b[i] = (double) (end - start);
	}

	close(fd);
}

static void *pagealign(void *b)
{
	int pgsz = getpagesize();
	void *x = (void *)(((unsigned long)(b) + pgsz - 1) & ~(pgsz - 1));
	return x;
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [--type={block|sg|bsg}]\n",
		osd_get_progname());
	exit(1);
}

int main(int argc, char *const *argv)
{
	int sz, i;
	void *x;
	double mhz = get_mhz();
	double mu, sd;
	const char *type = "block";
	void (*write_bw)(int iters, int sz);
	void (*read_bw)(int iters, int sz);
	struct option opt = {
	    .name = "type",
	    .has_arg = 1,
	    .val = 't',
	};

	const int iters = 100;
	const int maxsize = 512 * 1024;

	osd_set_progname(argc, argv);

	for (;;) {
		i = getopt_long(argc, argv, "t:", &opt, NULL);
		switch (i) {
		case -1:
			break;
		case '?':
			usage();
			break;
		case 't':
			type = optarg;
			break;
		}
		if (i == -1)
			break;
	}

	if (!strcmp(type, "block")) {
		write_bw = block_write_bw;
		read_bw = block_read_bw;
	} else if (!strcmp(type, "sg")) {
		write_bw = sg_write_bw;
		read_bw = sg_read_bw;
	} else if (!strcmp(type, "bsg")) {
		write_bw = bsg_write_bw;
		read_bw = bsg_read_bw;
	} else
		usage();
	printf("# %s type %s\n", osd_get_progname(), type);

	b = malloc(iters * sizeof(*b));
	x = malloc(maxsize + getpagesize());
	if (!b || !x)
		osd_error_fatal("%s: out of memory", __func__);

	buf = pagealign(x);
	memset(buf, 'A', maxsize);

	for (sz = 4096; sz <= maxsize; sz += 4096) {
		write_bw(iters, sz);
		for (i = 0; i < iters; i++)
			b[i] = sz / (b[i] / mhz);
		mu = mean(b, iters);
		sd = stddev(b, mu, iters);
		printf("write\t %3d %7.3lf +- %7.3lf\n", sz>>10, mu, sd);
	}

	printf("\n\n");

	for (sz = 4096; sz <= maxsize; sz += 4096) {
		read_bw(iters, sz);
		for (i = 0; i < iters; i++)
			b[i] = sz / (b[i] / mhz);
		mu = mean(b, iters);
		sd = stddev(b, mu, iters);
		printf("read\t %3d %7.3lf +- %7.3lf\n", sz>>10, mu, sd);
	}

	free(b);
	free(x);
	return 0;
}

