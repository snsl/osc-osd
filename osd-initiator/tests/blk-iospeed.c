/*
 * Better version of dd, for storage timing.  It is different in
 * that it does not advance the read/write offset as it goes, so
 * the target can stay in cache.  Also it will test all three
 * current ways to get the kernel to issue SCSI block commands.
 *
 * *** Be sure to edit /dev/sdb below!
 *
 * Copyright (C) 2007 Dennis Dalessandro (dennis@osc.edu)
 * Copyright (C) 2007 Ananth Devulapalli (ananth@osc.edu)
 * Copyright (C) 2007 Pete Wyckoff (pw@osc.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
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

#include "osd-util/osd-util.h"
#include <linux/bsg.h>

/*
 * Be sure to pick the disk correctly.  Write test will destroy it.
 */
#define BLOCK_DEV "/dev/sdb"

/*
 * Figure out /dev/bsg/... name from block device using sysfs.
 */
static const char *bsg_dev(const char *block_dev)
{
	const char *sd = block_dev + strlen("/dev/");
	char s[1024], t[1024], *cp;
	static char *u = NULL;
	int ret;

	if (u == NULL) {
		sprintf(s, "/sys/block/%s/queue/bsg", sd);
		ret = readlink(s, t, sizeof(t));
		if (ret < 0)
			osd_error_fatal("%s: readlink %s", __func__, s);
		t[ret] = 0;  /* dumb readlink api */
		cp = strrchr(t, '/') + 1;
		u = malloc(strlen("/dev/bsg/") + strlen(cp) + 1);
		sprintf(u, "/dev/bsg/%s", cp);
	}
	return u;
}

static double *b;
static void *buf;
static int pbsg_parallelism = 1;

static void block_write_bw(int iters, int bursts __attribute__((unused)),
			   int sz)
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

static void block_read_bw(int iters, int bursts __attribute__((unused)), int sz)
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

static void sg_write_bw(int iters, int bursts __attribute__((unused)), int sz)
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

static void sg_read_bw(int iters, int bursts __attribute__((unused)), int sz)
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

static void bsg_write_bw(int iters, int bursts __attribute__((unused)), int sz)
{
	int i, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_v4 bsg;
	const char *s = bsg_dev(BLOCK_DEV);

	fd = open(s, O_RDWR);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, s);

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

static void bsg_read_bw(int iters, int bursts __attribute__((unused)), int sz)
{
	int i, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_v4 bsg;
	const char *s = bsg_dev(BLOCK_DEV);

	fd = open(s, O_RDWR);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, s);

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

static void pbsg_write_bw(int iters, int bursts, int sz)
{
	int i, j, k, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_v4 bsg, bsgo;
	const char *s = bsg_dev(BLOCK_DEV);

	fd = open(s, O_RDWR);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, s);

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

	/* this many per timed burst */
	iters /= bursts;

	/* warm up */
	for (j=0; j<5; j++) {
		ret = ioctl(fd, SG_IO, &bsg);
		assert(ret == 0);
		assert(bsg.device_status == 0);
	}

	for (k=0; k<bursts; k++) {
		/* start all commands */
		rdtsc(start);
		for (j=0; j<pbsg_parallelism; j++) {
			bsg.usr_ptr = j;
			ret = write(fd, &bsg, sizeof(bsg));
			assert(ret == sizeof(bsg));
		}

		/* retire and repost until iters done */
		i = 0;
		while (i < iters) {
			ret = read(fd, &bsgo, sizeof(bsgo));
			assert(ret == sizeof(bsgo));
			assert(bsgo.device_status == 0);
			j = bsgo.usr_ptr;
			++i;
			if (iters - i >= pbsg_parallelism) {
				/* resubmit */
				bsg.usr_ptr = j;
				ret = write(fd, &bsg, sizeof(bsg));
				assert(ret == sizeof(bsg));
			}
		}
		rdtsc(end);
		b[k] = (double) (end - start) / iters;
	}

	close(fd);
}

static void pbsg_read_bw(int iters, int bursts, int sz)
{
	int i, j, k, ret, fd;
	uint64_t start, end;
	uint8_t cdb[10];
	uint16_t blocks;
	struct sg_io_v4 bsg, bsgo;
	const char *s = bsg_dev(BLOCK_DEV);

	fd = open(s, O_RDWR);
	if (fd < 0)
		osd_error_fatal("%s: cannot open %s", __func__, s);

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

	/* this many per timed burst */
	iters /= bursts;

	/* warm up */
	for (i=0; i<5; i++) {
		ret = ioctl(fd, SG_IO, &bsg);
		assert(ret == 0);
		assert(bsg.device_status == 0 && bsg.din_resid == 0);
	}

	for (k=0; k<bursts; k++) {
		/* start all commands */
		rdtsc(start);
		for (j=0; j<pbsg_parallelism; j++) {
			bsg.usr_ptr = j;
			ret = write(fd, &bsg, sizeof(bsg));
			assert(ret == sizeof(bsg));
		}

		/* retire and repost until iters done */
		i = 0;
		while (i < iters) {
			ret = read(fd, &bsgo, sizeof(bsgo));
			assert(ret == sizeof(bsgo));
			assert(bsgo.device_status == 0 && bsgo.din_resid == 0);
			j = bsgo.usr_ptr;
			++i;
			if (iters - i >= pbsg_parallelism) {
				/* resubmit */
				bsg.usr_ptr = j;
				ret = write(fd, &bsg, sizeof(bsg));
				assert(ret == sizeof(bsg));
			}
		}
		rdtsc(end);
		b[k] = (double) (end - start) / iters;
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
	fprintf(stderr, "Usage: %s [--type={block|sg|bsg|pbsg}] [--parallel=N] [--latency]\n",
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
	void (*write_bw)(int iters, int bursts, int sz);
	void (*read_bw)(int iters, int bursts, int sz);
	struct option opt[] = {
	{
	    .name = "type",
	    .has_arg = 1,
	    .val = 't',
	},
	{
	    .name = "parallel",
	    .has_arg = 1,
	    .val = 'p',
	},
	{
	    .name = "latency",
	    .val = 'l',
	},
	{ .name = NULL }
	};

	int maxsize, iters, bursts;
	int latency = 0;

	osd_set_progname(argc, argv);

	for (;;) {
		i = getopt_long(argc, argv, "t:p:l", opt, NULL);
		switch (i) {
		case -1:
			break;
		case '?':
			usage();
			break;
		case 't':
			type = optarg;
			break;
		case 'p':
			pbsg_parallelism = atoi(optarg);
			break;
		case 'l':
			latency = 1;
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
	} else if (!strcmp(type, "pbsg")) {
		write_bw = pbsg_write_bw;
		read_bw = pbsg_read_bw;
	} else
		usage();

	if (!strcmp(type, "pbsg"))
	    printf("# %s type %s %d\n", osd_get_progname(), type, pbsg_parallelism);
	else
	    printf("# %s type %s\n", osd_get_progname(), type);

	if (latency) {
		maxsize = 10 * 1024;
		iters = 1000;
	} else {
		maxsize = 512 * 1024;
		iters = 100;  /*** XXX: bigger is better for reporting */
	}

	bursts = iters;
	if (!strcmp(type, "pbsg"))
		bursts = 10;

	b = malloc(iters * sizeof(*b));
	x = malloc(maxsize + getpagesize());
	if (!b || !x)
		osd_error_fatal("%s: out of memory", __func__);

	buf = pagealign(x);
	memset(buf, 'A', maxsize);

	/* cannot make use of more than this */
	if (pbsg_parallelism > bursts)
		pbsg_parallelism = bursts;

	if (latency) {
		int step = 16;
		for (sz = 0; sz <= maxsize; sz += step) {
			if (!strcmp(type, "block"))  /* O_DIRECT */
				if (sz == 0 || (sz % 512) != 0)
					continue;
			write_bw(iters, bursts, sz);
			for (i = 0; i < bursts; i++)
				b[i] /= mhz;  /* time in us */
			mu = mean(b, bursts);
			sd = stddev(b, mu, bursts);
			printf("write\t %3d %7.3lf +- %7.3lf\n", sz, mu, sd);
		}

		printf("\n\n");

		for (sz = 0; sz <= maxsize; sz += step) {
			if (!strcmp(type, "block"))  /* O_DIRECT */
				if (sz == 0 || (sz % 512) != 0)
					continue;
			read_bw(iters, bursts, sz);
			for (i = 0; i < bursts; i++)
				b[i] /= mhz;
			mu = mean(b, bursts);
			sd = stddev(b, mu, bursts);
			printf("read\t %3d %7.3lf +- %7.3lf\n", sz, mu, sd);
		}
	} else {
		for (sz = 4096; sz <= maxsize; sz += 4096) {
			write_bw(iters, bursts, sz);
			for (i = 0; i < bursts; i++)
				b[i] = sz / (b[i] / mhz);  /* bw in MB/s */
			mu = mean(b, bursts);
			sd = stddev(b, mu, bursts);
			printf("write\t %3d %7.3lf +- %7.3lf\n", sz>>10,
			       mu, sd);
		}

		printf("\n\n");

		for (sz = 4096; sz <= maxsize; sz += 4096) {
			read_bw(iters, bursts, sz);
			for (i = 0; i < bursts; i++)
				b[i] = sz / (b[i] / mhz);
			mu = mean(b, bursts);
			sd = stddev(b, mu, bursts);
			printf("read\t %3d %7.3lf +- %7.3lf\n",
			       sz>>10, mu, sd);
		}
	}

	free(b);
	free(x);
	return 0;
}

