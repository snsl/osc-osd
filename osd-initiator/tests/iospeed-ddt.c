/*
 * IO throughput using SGL and Vectored.
 *
 * Copyright (C) 2007-8 OSD Team <pvfs-osd@osc.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"


enum {
	INVALID,
	WRITE,
	READ,
	FLUSH,
	NOFLUSH,
	CONTIG,
	VEC,
	SGL,
};

static uint64_t obj_create_any(int fd, uint64_t pid)
{
	struct osd_command command;
	struct attribute_list attr = {
		.type = ATTR_GET,
		.page = CUR_CMD_ATTR_PG,
		.number = CCAP_OID,
		.len = 8,
	};
	int ret;
	uint64_t oid;

	osd_command_set_create(&command, pid, 0, 0);
	osd_command_attr_build(&command, &attr, 1);
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit_and_wait failed", __func__);
		exit(1);
	}
	ret = osd_command_attr_resolve(&command);
	if (ret) {
		osd_error_xerrno(ret, "%s: attr_resolve failed", __func__);
		exit(1);
	}
	oid = get_ntohll(command.attr[0].val);
	osd_command_attr_free(&command);
	return oid;
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n", osd_get_progname());
	fprintf(stderr, "There are two main ways to run this program\n");
	fprintf(stderr, "	1. Using the default data distribution (contiguous)\n");
	fprintf(stderr, "		-o [read|write]\n");
	fprintf(stderr, "		-l [max size]\n");
	fprintf(stderr, "	2. Using a specialized data distribution\n");
	fprintf(stderr, "		-o [read|write]\n");
	fprintf(stderr, "		-d [sgl|vec]\n");
	fprintf(stderr, "		-l [max size]\n");
	fprintf(stderr, "		-s [stride]\n");
	fprintf(stderr, "		-n [number bytes per segment]\n");
	fprintf(stderr, "Both can use the following options:\n");
	fprintf(stderr, "		-t [trials]\n");
	fprintf(stderr, "		-f Flush writes to disk\n");
	fprintf(stderr, "		-i Start test at 4K and increasing by i up to max size\n");
	fprintf(stderr, "		-w [warm up]\n");
	fprintf(stderr, "*For size values a k, m, or g can be appended for KB, MB, or GB ie 100k\n");
	exit(1);
}

static void obj_remove(int fd, uint64_t pid, uint64_t oid)
{
	struct osd_command command;
	int ret;

	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit_and_wait failed", __func__);
		exit(1);
	}
}

static uint64_t parse_number(const char *cp)
{
	uint64_t v;
	char *cq;

	v = strtoul(cp, &cq, 0);
	if (*cq) {
		if (!strcasecmp(cq, "k"))
			v <<= 10;
		else if (!strcasecmp(cq, "m"))
			v <<= 20;
		else if (!strcasecmp(cq, "g"))
			v <<= 30;
		else
			usage();
	}
	return v;
}

static void show_stats(double *bw_array, int trials, uint64_t size, int mode,
		       int flush)
{
	double mu, sd;
	const char *s;
	
	mu = mean(bw_array, trials);
	sd = stddev(bw_array, mu, trials);
	s = "read";
	if (mode == WRITE) {
		if (flush == FLUSH)
			s = "write-flush";
		else
			s = "write";
	}
	printf("%-11s %3zu kB %7.3lf +- %7.3lf MB/s\n", s, size>>10, mu, sd);
}

static void do_contig(int fd, int mode, uint64_t pid, uint64_t oid, int trials,
		      uint64_t size, uint64_t numbytes, uint64_t stride,
		      int flush, int warmup)
{

	uint64_t start, stop, delta;
	int ret;
	void *buf;
	double *bw_array;
	double mhz = get_mhz();
	double time;
	int i, j;
	uint64_t offset;
	uint64_t total_size;
	int segments;
	int bytes_left;

	assert(mode != READ && flush != FLUSH);

	bw_array = malloc(trials * sizeof(*bw_array));
	assert(bw_array != NULL);
	buf = malloc(size);
	assert(buf != NULL);
	memset(buf, 'X', size); /* fill with something */

	if (mode == READ)
		/* put something there to read */
		ret = write_osd(fd, pid, oid, buf, size, 0);

	if (numbytes == 0 || stride == 0)
		/* default to a single write */
		numbytes = size;

	/* figure out how many segments we are going to have */
	segments = size / numbytes;
	bytes_left = size % numbytes;
	total_size = segments * numbytes + bytes_left;
	if (bytes_left > 0)
		/* need one last segment for the remainder of bytes */
		++segments;
	assert(total_size == size);

	for (i = 0; i < trials+warmup; i++) {
		offset = 0;
		rdtsc(start);
		for (j = 0; j < segments; j++) {
			if (j + 1 == segments && bytes_left != 0) {
				if (mode == WRITE)
					ret = write_osd(fd, pid, oid, buf, bytes_left, offset);
				else
					ret = read_osd(fd, pid, oid, buf, bytes_left, offset);
				assert(ret == 0);
			} else {
				if (mode == WRITE)
					ret = write_osd(fd, pid, oid, buf, numbytes, offset);
				else
					ret = read_osd(fd, pid, oid, buf, numbytes, offset);
				assert(ret == 0);
				offset += stride;
			}
		}
		rdtsc(stop);

		delta = stop - start;

		if (flush == FLUSH) {
			rdtsc(start);
			ret = flush_object(fd, pid, oid, 0 ,0 ,0);
			rdtsc(stop);
			assert(ret == 0);
			delta += stop - start;
		}
		time = ((double)delta)/mhz; /* time in usec */
		if (i >= warmup)
			bw_array[i-warmup] = size/time; /* BW in MegaBytes/sec */
	}

	show_stats(bw_array, trials, size, mode, flush);
	free(buf);
	free(bw_array);
}

static void do_sgl(int fd, int mode, uint64_t pid, uint64_t oid, int trials,
			uint64_t size, uint64_t numbytes, uint64_t stride,
			int flush, int warmup)
{
	uint64_t start, stop, delta;
	int ret;
	void *buf, *ret_buf = NULL;
	double *bw_array;
	double mhz = get_mhz();
	double time;
	int i;
	uint64_t total_size;
	int segments;
	int bytes_left;
	uint64_t offset, hdr_offset;
	uint64_t ddt_size;

	if (mode == READ)
		assert(flush != FLUSH);

	bw_array = malloc(trials * sizeof(*bw_array));
	assert(bw_array != NULL);

	if (mode == READ) {
		ret_buf = malloc(size);
		assert(ret_buf != NULL);
		memset(ret_buf, 'X', size);
	}

	/* figure out how many segments we are going to have */
	segments = size / numbytes;
	bytes_left = size % numbytes;
	total_size = segments * numbytes + bytes_left;
	if (bytes_left > 0)
		/* need one last segment for the remainder of bytes */
		++segments;
	assert(total_size == size);

	/* to rep each segment need 2 uint64_ts, and need #segments */
	ddt_size = (sizeof(uint64_t) * segments * 2) + sizeof(uint64_t);
	total_size += ddt_size;

	if (mode == WRITE) {
		buf = malloc(total_size);
		assert(buf != NULL);
		memset(buf, 'X', total_size); /* fill with something */
	} else {
		buf = malloc(ddt_size);
		assert(buf != NULL);
	}

	/* prepare the buffer:  |#segments|offset|len|offset|len|....|DATA| */
	hdr_offset = 0;
	offset = 0;
	set_htonll(buf, segments);
	hdr_offset += sizeof(uint64_t);
	for (i = 0; i < segments; i++) {
		set_htonll((uint8_t *)buf + hdr_offset, offset);
		offset += stride;
		hdr_offset += sizeof(uint64_t);
		if (i + 1 == segments && bytes_left != 0)
			set_htonll((uint8_t *)buf + hdr_offset, bytes_left);
		else
			set_htonll((uint8_t *)buf + hdr_offset, numbytes);
		hdr_offset += sizeof(uint64_t);
	}


	if (mode == READ)
		/* put something there to read */
		ret = write_osd(fd, pid, oid, ret_buf, size, 0);

	for (i = 0; i < trials+warmup; i++) {
		rdtsc(start);
		if (mode == WRITE)
			ret = write_sgl_osd(fd, pid, oid, buf, size, 0);
		else
			ret = read_sgl_osd(fd, pid, oid, buf, ddt_size, ret_buf,
					  size, 0);

		rdtsc(stop);
		assert(ret == 0);
		delta = stop - start;

		if (flush == FLUSH) {
			rdtsc(start);
			ret = flush_object(fd, pid, oid, 0, 0, 0);
			rdtsc(stop);
			assert(ret == 0);
			delta += stop - start;
		}
		time = ((double)delta)/mhz; /* time in usec */
		if (i >= warmup)
			bw_array[i-warmup] = size/time; /* BW in MegaBytes/sec */
	}

	show_stats(bw_array, trials, size, mode, flush);
	free(buf);
	free(bw_array);
}

static void do_vec(int fd, int mode, uint64_t pid, uint64_t oid, int trials,
			uint64_t size, uint64_t numbytes, uint64_t stride,
			int flush, int warmup)
{
	uint64_t start, stop, delta;
	int ret;
	void *buf, *ret_buf = NULL;
	double *bw_array = NULL;
	double mhz = get_mhz();
	double time = 0.0;
	int i;
	uint64_t total_size;
	uint64_t hdr_offset = 0;
	uint64_t ddt_size = 0;

	if (mode == READ)
		assert(flush != FLUSH);

	bw_array = malloc(trials * sizeof(*bw_array));
	assert(bw_array != NULL);

	if (mode == READ) {
		ret_buf = malloc(size);
		assert(ret_buf != NULL);
		memset(ret_buf, 'X', size);
	}

	ddt_size = (sizeof(uint64_t) * 2);
	total_size = ddt_size + size;

	if (mode == WRITE) {
		buf = malloc(total_size);
		assert(buf != NULL);
		memset(buf, 'X', total_size); /* fill with something */
	} else {
		buf = malloc(ddt_size);
		assert(buf != NULL);
	}

	/* prepare the buffer:  |stride|len|DATA| */

	set_htonll(buf, stride);
	hdr_offset = sizeof(uint64_t);
	set_htonll((uint8_t *)buf + hdr_offset, numbytes);

	if (mode == READ)
		/* put something there to read */
		ret = write_osd(fd, pid, oid, ret_buf, size, 0);

	for (i = 0; i < trials+warmup; i++) {
		rdtsc(start);
		if (mode == WRITE)
			ret = write_vec_osd(fd, pid, oid, buf, total_size, 0);
		else
			ret = read_vec_osd(fd, pid, oid, buf, ddt_size, ret_buf,
					   size, 0);

		rdtsc(stop);
		assert(ret == 0);
		delta = stop - start;

		if (flush == FLUSH) {
			rdtsc(start);
			ret = flush_object(fd, pid, oid, 0, 0, 0);
			rdtsc(stop);
			assert(ret == 0);
			delta += stop - start;
		}
		time = ((double)delta)/mhz; /* time in usec */
		if (i >= warmup)
			bw_array[i-warmup] = size/time; /* BW in MegaBytes/sec */
	}

	show_stats(bw_array, trials, size, mode, flush);
	free(buf);
	free(bw_array);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;
	uint64_t oid, pid;
	int c;
	uint64_t length = 0;
	uint64_t stride = 0;
	uint64_t num = 0;
	int trials = 10;
	int warmup = 5;
	uint64_t size;
	uint64_t iter = 0;
	int mode = INVALID;
	int flush = NOFLUSH;
	int ddt = CONTIG;

	osd_set_progname(argc, argv);

	/* note: when passing params ensure end up with a contiguous file
	   especially for reads or else we won't be able to stage a file ahead
	   of time */

	while ((c = getopt (argc, argv, "l:o:d:s:n:t:i:w:f")) != -1)
		switch (c) {
			case 'l':
				length = parse_number(optarg);
				break;
			case 'o':
				if (!strcmp(optarg, "write"))
					mode = WRITE;
				else if (!strcmp(optarg, "read"))
					mode = READ;
				else
					usage();
				break;
			case 'd':
				if (!strcmp(optarg, "sgl"))
					ddt = SGL;
				else if (!strcmp(optarg, "vec"))
					ddt = VEC;
				break;
			case 's':
				stride = parse_number(optarg);
				break;
			case 'n':
				num = parse_number(optarg);
				break;
			case 'f':
				flush = FLUSH;
				break;
			case 't':
				trials = atoi(optarg);
				break;
			case 'i':
				iter = parse_number(optarg);
				break;
			case 'w':
				warmup = atoi(optarg);
				break;
			default:
				usage();
		}

	if (mode == INVALID) {
		fprintf(stderr, "mode is invalid\n");
		usage();
	}

	if (mode == READ && flush == FLUSH) {
		fprintf(stderr, "flush has no meaning for writes \n");
		usage();
	}

	if (ddt != CONTIG && num <= 0) {
		fprintf(stderr, "num segments required when specifying ddt\n");
		usage();
	}

	if (ddt != CONTIG && stride == 0) {
		fprintf(stderr, "Stride length must be specifyed\n");
		usage();
	}

	if (ddt != CONTIG && stride < num) {
		fprintf(stderr, "Stride must be at least as big as num\n");
		usage();
	}


	if (trials <= 0) {
		fprintf(stderr, "problem with number of trials\n");
		usage();
	}

	if (iter > 0 && length <= 4096) {
		fprintf(stderr, "if using an iter value total length must be > 4k\n");
		usage();
	}

	if (length <= (unsigned)iter) {
		fprintf(stderr, "length must be bigger than iter value\n");
		usage();
	}

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		osd_error("%s: get drive error", __func__);
		return 1;
	}
	if (num_drives == 0) {
		osd_error("%s: no drives", __func__);
		return 1;
	}

	i = 0;
	osd_debug("drive %s name %s", drives[i].chardev, drives[i].targetname);
	fd = open(drives[i].chardev, O_RDWR);
	if (fd < 0) {
		osd_error_errno("%s: open %s", __func__, drives[i].chardev);
		return 1;
	}
	osd_free_drive_list(drives, num_drives);

	inquiry(fd);

	format_osd(fd, 1<<30);

	pid = PARTITION_PID_LB;
	create_partition(fd, pid);

	oid = obj_create_any(fd, pid);

	for (size = 4096; size <= length; size += iter) {
		/* run test */
		switch (ddt) {
			case CONTIG:
				do_contig(fd, mode, pid, oid, trials, size,
					  num, stride, flush, warmup);
				break;
			case SGL:
				do_sgl(fd, mode, pid, oid, trials, size, num,
					stride, flush, warmup);
				break;
			case VEC:
				do_vec(fd, mode, pid, oid, trials, size, num,
					stride, flush, warmup);
				break;
			default:
				exit(1);  /* should never happen */
		}
	}

	obj_remove(fd, pid, oid);
	close(fd);
	return 0;
}
