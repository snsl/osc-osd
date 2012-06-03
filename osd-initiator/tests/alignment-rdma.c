/*
 * Probe performance issues when iser rdma alignment violations occur.
 *
 * Copyright (C) 2008 Pete Wyckoff <pw@osc.edu>
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

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
	osd_command_attr_resolve(&command);
	oid = get_ntohll(command.attr[0].val);
	osd_command_attr_free(&command);
	return oid;
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


/*
 * For various amounts of discontiguity, compare rdma-realigned performance
 * vs multiple transfers vs memcpy in userspace;
 */
static void una_write_test(int fd, uint64_t pid)
{
	struct osd_command command, *commandp;
	struct bsg_iovec *vec;
	int ret, i, j, numvec;
	uint64_t oid;
	uint32_t onesize, pos;
	uint8_t *buf, *linbuf;
	double mu, sd;
	double *v;
	uint64_t start, end;
	int pagesize = getpagesize();
	const int numpages = (256 << 10) / pagesize;
	const uint32_t buflen = numpages * pagesize;
	const int iter = 1000, warmup = 10;

	printf("write numpages %d buflen %d\n", numpages, buflen);
	v = malloc((iter + warmup) * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	/* source buf */
	ret = posix_memalign((void *) &buf, pagesize, buflen + pagesize);
	if (ret) {
		perror("malloc");
		exit(1);
	}
	memset(buf, 0x5a, buflen + pagesize);
	buf += pagesize / 2;  /* force unaligned */

	/* linear buf */
	ret = posix_memalign((void *) &linbuf, pagesize, buflen);
	if (!buf) {
		perror("malloc linbuf");
		exit(1);
	}

	/* iovec */
	vec = malloc(numpages * sizeof(*vec));

	oid = obj_create_any(fd, pid);

	for (numvec=1; numvec<=numpages; numvec *= 2) {

		/* build iovec */
		onesize = buflen / numvec;
		pos = 0;

		for (i = numvec-1; i >= 0; i--) {
			vec[i].iov_base = (uintptr_t) buf + pos;
			vec[i].iov_len = onesize;
			pos += onesize;
		}

		/* contig */
		osd_command_set_write(&command, pid, oid, buflen, 0);
		command.outlen = buflen;
		command.outdata = buf;
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			ret = osd_submit_and_wait(fd, &command);
			rdtsc(end);
			assert(ret == 0);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("write contig %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);

		/* iovec, with memcpy in driver */
		osd_command_set_write(&command, pid, oid, buflen, 0);
		command.outlen = buflen;
		command.outdata = vec;
		command.iov_outlen = numvec;
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			ret = osd_submit_and_wait(fd, &command);
			rdtsc(end);
			assert(ret == 0);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("write iovec  %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);

		/* memcpy here */
		osd_command_set_write(&command, pid, oid, buflen, 0);
		command.outlen = buflen;
		command.outdata = linbuf;
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			pos = 0;
			for (j = numvec-1; j >= 0; j--) {
				memcpy(&linbuf[pos], (void *) vec[j].iov_base,
				       vec[j].iov_len);
				pos += vec[j].iov_len;
			}
			ret = osd_submit_and_wait(fd, &command);
			rdtsc(end);
			assert(ret == 0);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("write memcpy %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);

		/* multiple xfers, all at the same time */
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			pos = 0;
			for (j = numvec-1; j >= 0; j--) {
				osd_command_set_write(&command, pid, oid,
						      onesize, pos);
				command.outlen = onesize;
				command.outdata = (void *) vec[j].iov_base;
				ret = osd_submit_command(fd, &command);
				assert(ret == 0);
				pos += vec[j].iov_len;
			}
			for (j = numvec-1; j >= 0; j--) {
				ret = osd_wait_response(fd, &commandp);
				assert(ret == 0);
			}
			rdtsc(end);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("write multi  %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);
	}

	obj_remove(fd, pid, oid);
	free(vec);
	free(linbuf);
	free(buf - pagesize/2);
	free(v);
}

static void una_read_test(int fd, uint64_t pid)
{
	struct osd_command command, *commandp;
	struct bsg_iovec *vec;
	int ret, i, j, numvec;
	uint64_t oid;
	uint32_t onesize, pos;
	uint8_t *buf, *linbuf;
	double mu, sd;
	double *v;
	uint64_t start, end;
	int pagesize = getpagesize();
	const int numpages = (256 << 10) / pagesize;
	const uint32_t buflen = numpages * pagesize;
	const int iter = 1000, warmup = 10;

	printf("read  numpages %d buflen %d\n", numpages, buflen);
	v = malloc((iter + warmup) * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	/* source buf */
	ret = posix_memalign((void *) &buf, pagesize, buflen + pagesize);
	if (ret) {
		perror("malloc");
		exit(1);
	}
	memset(buf, 0x5a, buflen + pagesize);
	buf += pagesize / 2;  /* force unaligned */

	/* linear buf */
	ret = posix_memalign((void *) &linbuf, pagesize, buflen);
	if (!buf) {
		perror("malloc linbuf");
		exit(1);
	}

	/* iovec */
	vec = malloc(numpages * sizeof(*vec));

	oid = obj_create_any(fd, pid);

	/* give it some data */
	osd_command_set_write(&command, pid, oid, buflen, 0);
	command.outlen = buflen;
	command.outdata = buf;
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	for (numvec=1; numvec<=numpages; numvec *= 2) {

		/* build iovec */
		onesize = buflen / numvec;
		pos = 0;

		for (i = numvec-1; i >= 0; i--) {
			vec[i].iov_base = (uintptr_t) buf + pos;
			vec[i].iov_len = onesize;
			pos += onesize;
		}

		/* contig */
		osd_command_set_read(&command, pid, oid, buflen, 0);
		command.inlen_alloc = buflen;
		command.indata = buf;
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			ret = osd_submit_and_wait(fd, &command);
			rdtsc(end);
			assert(ret == 0);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("read  contig %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);

		/* iovec, with memcpy in driver */
		osd_command_set_read(&command, pid, oid, buflen, 0);
		command.inlen_alloc = buflen;
		command.indata = vec;
		command.iov_inlen = numvec;
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			ret = osd_submit_and_wait(fd, &command);
			rdtsc(end);
			assert(ret == 0);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("read  iovec  %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);

		/* memcpy here */
		osd_command_set_read(&command, pid, oid, buflen, 0);
		command.inlen_alloc = buflen;
		command.indata = linbuf;
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			ret = osd_submit_and_wait(fd, &command);
			pos = 0;
			for (j = numvec-1; j >= 0; j--) {
				memcpy((void *) vec[j].iov_base, &linbuf[pos],
				       vec[j].iov_len);
				pos += vec[j].iov_len;
			}
			rdtsc(end);
			assert(ret == 0);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("read  memcpy %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);

		/* multiple xfers, all at the same time */
		for (i=0; i<iter+warmup; i++) {
			rdtsc(start);
			pos = 0;
			for (j = numvec-1; j >= 0; j--) {
				osd_command_set_read(&command, pid, oid,
						     onesize, pos);
				command.inlen_alloc = onesize;
				command.indata = (void *) vec[j].iov_base;
				ret = osd_submit_command(fd, &command);
				assert(ret == 0);
				pos += vec[j].iov_len;
			}
			for (j = numvec-1; j >= 0; j--) {
				ret = osd_wait_response(fd, &commandp);
				assert(ret == 0);
			}
			rdtsc(end);
			v[i] = (double) (end - start) / mhz;  /* in us */
		}
		mu = mean(v + warmup, iter);
		sd = stddev(v + warmup, mu, iter);
		printf("read  multi  %2d %9.3lf +- %9.3lf\n", numvec, mu, sd);
	}

	obj_remove(fd, pid, oid);
	free(vec);
	free(linbuf);
	free(buf - pagesize/2);
	free(v);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;

	osd_set_progname(argc, argv);
	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n", osd_get_progname());
		return 1;
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
	printf("%s: drive %s name %s\n", osd_get_progname(), drives[i].chardev,
	       drives[i].targetname);
	fd = open(drives[i].chardev, O_RDWR);
	if (fd < 0) {
		osd_error_errno("%s: open %s", __func__, drives[i].chardev);
		return 1;
	}
	osd_free_drive_list(drives, num_drives);

	format_osd(fd, 1<<30);
	create_partition(fd, PARTITION_PID_LB);

	una_write_test(fd, PARTITION_PID_LB);
	una_read_test(fd, PARTITION_PID_LB);

	close(fd);
	return 0;
}

