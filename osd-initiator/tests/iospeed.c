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

#include "util/osd-util.h"
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
	ret = osd_command_attr_resolve(&command);
	if (ret) {
		osd_error_xerrno(ret, "%s: attr_resolve failed", __func__);
		exit(1);
	}
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

static void read_bw(int fd, uint64_t pid, uint64_t oid, 
		    size_t sz, int iters, int dosync)
{
	int i = 0;
	int ret = 0;
	uint64_t start, end, delta;
	double mhz = get_mhz();
	double time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;

	buf = malloc(sz);
	b = malloc(iters * sizeof(*b));

	if (!buf || !b)
		osd_error_fatal("out of memory");

	/* warm up */
	for (i=0; i<5; i++) {
		ret = read_osd(fd, pid, oid, buf, sz, 0);
		assert(ret == 0);
	}

	for (i=0; i< iters; i++) {
		if (dosync) {
			rdtsc(start);
			ret = read_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;

			rdtsc(start);
			ret = flush_object(fd, pid, oid, 2);
			rdtsc(end);
			assert(ret == 0);
			delta += (end - start);
		} else {
			rdtsc(start);
			ret = read_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;
		}

		time = ((double)delta)/mhz; /* time in usec */
		b[i] = sz/time; /* BW in MegaBytes/sec */
	}

	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	if (dosync)
		printf("read-sync  %3zu %7.3lf +- %7.3lf\n", sz>>10, mu, sd);
	else 
		printf("read       %3zu %7.3lf +- %7.3lf\n", sz>>10, mu, sd);

/* 	uint8_t *p = buf;
	printf("%x %x %x %x %x\n", p[0], p[127], p[sz-256], p[sz-2], p[sz-1]); */
	free(buf);
	free(b);
}

static void write_bw(int fd, uint64_t pid, uint64_t oid, 
		     size_t sz, int iters, int dosync)
{
	int i = 0;
	int ret = 0;
	uint64_t start, end, delta;
	double mhz = get_mhz();
	double time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;
	/* uint8_t *p = NULL; */

	buf = malloc(sz);
	b = malloc(iters * sizeof(*b));

	if (!buf || !b)
		osd_error_fatal("out of memory");

#if 0
	assert(sz > 256);
	
	p = buf;
	memset(p, 0xde, 128); 
	p[0] = 0;
	p += 128;
	memset(p, 0xa5, sz - 256);
	p += (sz - 256);
	memset(p, 0xef, 128);
	p[127] = 0;
#endif

	/* warm up */
	for (i=0; i< 5; i++) {
		ret = write_osd(fd, pid, oid, buf, sz, 0);
		assert(ret == 0);
	}

	for (i=0; i< iters; i++) {
		if (dosync) {
			rdtsc(start);
			ret = write_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;

			rdtsc(start);
			/* XXX: this may be broken, numbers reported
			 * are too big to be reasonable */
			ret = flush_object(fd, pid, oid, 2);
			rdtsc(end);
			assert(ret == 0);

			delta += end - start;
		} else {
			rdtsc(start);
			ret = write_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;
		}

		time = ((double)delta)/mhz; /* time in usec */
		b[i] = sz/time; /* BW in MegaBytes/sec */
	}

	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	if (dosync)
		printf("write-sync %3zu %7.3lf +- %7.3lf\n", sz>>10, mu, sd);
	else 
		printf("write      %3zu %7.3lf +- %7.3lf\n", sz>>10, mu, sd);

	free(buf);
	free(b);
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [<size in kbytes>]\n", osd_get_progname());
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;
	const int iter = 100;
	uint64_t oid;
	int onesize = 0;

	osd_set_progname(argc, argv); 

	if (argc == 1) {
		;
	} else if (argc == 2) {
		char *cp;
		onesize = strtoul(argv[1], &cp, 0);
		if (*cp != '\0')
			usage();
		onesize <<= 10;
	} else {
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
	create_partition(fd, PARTITION_PID_LB);

	oid = obj_create_any(fd, PARTITION_PID_LB);

#if 0
	for (i=(1<<19); i<=(1<<19); i <<= 1) {
		printf("write/read %d\n", i);
		write_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
		read_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
	}
#else
 	printf("# osd_initiator/tests/iospeed\n");
	printf("# type  size (kB)  rate (MB/s) +- stdev\n");
	if (onesize) {
		write_bw(fd, PARTITION_PID_LB, oid, onesize, iter, 0);
		read_bw(fd, PARTITION_PID_LB, oid, onesize, iter, 0);
	} else {
		for (i=4096; i<(1<<19); i+=4096)
			write_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
		printf("\n\n");
		for (i=4096; i<(1<<19); i+=4096)
			read_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
		printf("\n\n");
#if 0
		for (i=4096; i<=(1<<19); i+=4096)
			write_bw(fd, PARTITION_PID_LB, oid, i, iter, 1);
		printf("\n\n");
		for (i=4096; i<=(1<<19); i+=4096)
			read_bw(fd, PARTITION_PID_LB, oid, i, iter, 1);
		printf("\n\n");
#endif
	}
#endif

	obj_remove(fd, PARTITION_PID_LB, oid);

	close(fd);
	return 0;
}

