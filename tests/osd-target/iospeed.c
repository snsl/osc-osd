#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include "util/osd-defs.h"
#include "target-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util/util.h"
#include "util/osd-sense.h"
#include "target-sense.h"

#if defined(__x86_64__)
	#define rdtsc(v) do { \
        	unsigned int __a, __d; \
        	asm volatile("rdtsc" : "=a" (__a), "=d" (__d)); \
        	(v) = ((unsigned long)__a) | (((unsigned long)__d)<<32); \
	} while(0)
#elif defined(__i386__)
	#define rdtsc(v) do { \
		asm volatile("rdtsc" : "=A" (v)); \
	} while (0)
#endif

double mean(double *v, int N);
double stddev(double *v, double mu, int N);
double get_mhz(void);
void write_bw(struct osd_device *osd, uint64_t pid, uint64_t oid, 
	      uint8_t *sense, size_t sz, int iters, int dosync);
void read_bw(struct osd_device *osd, uint64_t pid, uint64_t oid, 
	     uint8_t *sense, size_t sz, int iters, int dosync);

double mean(double *v, int N)
{
	int i = 0;
	double mu = 0.0;

	for (i = 0; i < N; i++)
		mu += v[i];

	return mu/N;
}

double stddev(double *v, double mu, int N) 
{
	int i = 0;
	double sd = 0.0;

	for (i = 0; i < N; i++)
		sd += (v[i] - mu)*(v[i] - mu);

	return sqrt(sd/(N - 1));
}

double get_mhz(void)
{
	FILE *fp;
	char s[1024], *cp;
	int found = 0;
	double mhz;

	if (!(fp = fopen("/proc/cpuinfo", "r")))
		osd_error_fatal("Cannot open /proc/cpuinfo");

	while (fgets(s, sizeof(s), fp)) {
		if (!strncmp(s, "cpu MHz", 7)) {
			found = 1;
			for (cp=s; *cp && *cp != ':'; cp++) ;
			if (!*cp)
				osd_error_fatal("no colon found in string");
			++cp;
			if (sscanf(cp, "%lf", &mhz) != 1)
				osd_error_fatal("scanf got no value");
		}
	}
	if (!found)
		osd_error_fatal("\"cpu MHz\" line not found\n");

	fclose(fp); 

	return mhz;
}

void read_bw(struct osd_device *osd, uint64_t pid, uint64_t oid, 
	     uint8_t *sense, size_t sz, int iters, int dosync)
{
	int i = 0;
	int ret = 0;
	uint64_t used;
	uint64_t start, end, delta;
	double mhz = get_mhz();
	double time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;
	const uint8_t ssk = OSD_SSK_RECOVERED_ERROR;
	const uint16_t asc = OSD_ASC_READ_PAST_END_OF_USER_OBJECT;

	buf = malloc(sz);
	b = malloc(iters * sizeof(*b));

	if (!buf || !b)
		osd_error_fatal("out of memory");

	/* warm up */
	for (i=0; i< 5; i++) {
		ret = osd_read(osd, pid, oid, sz, 0, buf, &used, sense);
		assert(ret >= 0);
		if (ret > 0) {
			assert(sense_test_type(sense, ssk, asc));
			assert(ntohll(&sense[44]) == sz);
		}
	}

	for (i=0; i< iters; i++) {
		if (dosync) {
			rdtsc(start);
			ret = osd_read(osd, pid, oid, sz, 0, buf, &used, sense);
			rdtsc(end);
			assert(ret >= 0);
			if (ret > 0) {
				assert(sense_test_type(sense, ssk, asc));
				assert(ntohll(&sense[44]) == sz);
			}
			delta = end - start;

			rdtsc(start);
			ret = osd_flush(osd, pid, oid, 0, sense);
			rdtsc(end);
			assert(ret == 0);
			delta += (end - start);
		} else {
			rdtsc(start);
			ret = osd_read(osd, pid, oid, sz, 0, buf, &used, sense);
			rdtsc(end);
			assert(ret >= 0);
			if (ret > 0) {
				assert(sense_test_type(sense, ssk, asc));
				assert(ntohll(&sense[44]) == sz);
			}
			delta = end - start;
		}

		time = ((double)delta)/mhz; /* time in usec */
		b[i] = sz/time; /* BW in MegaBytes/sec */
	}

	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	if (dosync)
		osd_info("read sync %lu %lf +- %lf", sz, mu, sd);
	else 
		osd_info("read %lu %lf +- %lf", sz, mu, sd);

	free(buf);
	free(b);
}

void write_bw(struct osd_device *osd, uint64_t pid, uint64_t oid, 
	      uint8_t *sense, size_t sz, int iters, int dosync)
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
	for (i=0; i< 5; i++) {
		ret = osd_write(osd, pid, oid, sz, 0, buf, sense);
		assert(ret == 0);
	}

	for (i=0; i< iters; i++) {
		if (dosync) {
			rdtsc(start);
			ret = osd_write(osd, pid, oid, sz, 0, buf, sense);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;

			rdtsc(start);
			ret = osd_flush(osd, pid, oid, 2, sense);
			rdtsc(end);
			assert(ret == 0);

			delta += end - start;
		} else {
			rdtsc(start);
			ret = osd_write(osd, pid, oid, sz, 0, buf, sense);
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
		osd_info("write sync %lu %lf +- %lf", sz, mu, sd);
	else 
		osd_info("write %lu %lf +- %lf", sz, mu, sd);

	free(buf);
	free(b);
}

int main()
{
	int i = 0;
	int ret = 0;
	int iter = 10;
	const char *root = "/tmp/osd/";
	struct osd_device osd;
	uint8_t sense[OSD_MAX_SENSE];

	ret = osd_open(root, &osd);
	assert(ret == 0);

	ret = osd_create_partition(&osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	ret = osd_create(&osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	assert(ret == 0);

	for (i=12; i<28; i++)
		write_bw(&osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense,
			 1 << i, iter, 0);
	for (i=12; i<28; i++)
		write_bw(&osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense,
			 1 << i, iter, 1);
	for (i=12; i<28; i++)
		read_bw(&osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense,
			1 << i, iter, 0);

	for (i=12; i<28; i++)
		read_bw(&osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense,
			1 << i, iter, 1);

	ret = osd_remove(&osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	assert(ret == 0);

	ret = osd_remove_partition(&osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	ret = osd_close(&osd);
	assert(ret == 0);

	return 0;
}
