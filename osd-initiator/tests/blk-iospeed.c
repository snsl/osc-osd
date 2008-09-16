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

static void *pagealign(void *b)
{
	int pgsz = getpagesize();
	void *x = (void *)(((unsigned long)(b) + pgsz - 1) & ~(pgsz - 1));
	return x;
}


static void read_bw(int iters, size_t sz)
{
	int i = 0;
	int ret = 0;
	int fd = 0;
	uint64_t start, end, delta;
	double mhz = get_mhz();
	double time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;
	void *x = NULL;

	x = malloc(sz + 4096);
	b = malloc(iters * sizeof(*b));
	if (!b || !x)
		osd_error_fatal("%s: out of memory", __func__);

	buf = pagealign(x);

	fd = open("/dev/sdb", O_RDONLY|O_DIRECT);
	if (fd < 0)
		osd_error_fatal("%s: cannot open /dev/sdb", __func__);

	/* warm up */
	for (i=0; i<5; i++) {
		ret = read(fd, buf, sz);
		assert(ret == sz);
	}

	for (i=0; i< iters; i++) {
		rdtsc(start);
		ret = read(fd, buf, sz);
		rdtsc(end);
		assert(ret == sz);
		delta = end - start;
		time = ((double)delta)/mhz;
		b[i] = sz/time;
	}

	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	printf("read\t %3lu %7.3lf +- %7.3lf\n", sz>>10, mu, sd);

	free(x);
	free(b);
}


static void write_bw(int iters, size_t sz)
{
	int i = 0;
	int ret = 0;
	int fd = 0;
	uint64_t start, end, delta;
	double mhz = get_mhz();
	double time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;
	void *x = NULL;

	x = malloc(sz + 4096);
	b = malloc(iters * sizeof(*b));
	if (!b || !x)
		osd_error_fatal("%s: out of memory", __func__);

	buf = pagealign(x);
	memset(buf, 'A', sz);

	fd = open("/dev/sdb", O_WRONLY|O_DIRECT);
	if (fd < 0)
		osd_error_fatal("%s: cannot open /dev/sdb", __func__);

	/* warm up */
	for (i=0; i<5; i++) {
		ret = write(fd, buf, sz);
		assert(ret == sz);
	}

	for (i=0; i< iters; i++) {
		rdtsc(start);
		ret = write(fd, buf, sz);
		rdtsc(end);
		assert(ret == sz);
		delta = end - start;
		time = ((double)delta)/mhz;
		b[i] = sz/time;
	}

	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	printf("write\t %3lu %7.3lf +- %7.3lf\n", sz>>10, mu, sd);

	free(x);
	free(b);
}

int main()
{
	int i;
	int iters = 100;

	for (i = 4096; i < 512*1024; i += 4096)
		write_bw(iters, i);

	printf("\n\n");

	for (i = 4096; i < 512*1024; i += 4096)
		read_bw(iters, i);

	return 0;
}

