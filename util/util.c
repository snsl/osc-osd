/*
 * Copyright (C) 2000-7 Pete Wyckoff <pw@osc.edu>
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#include "util.h"

#define __unused __attribute__((unused))

/* global */
const char *progname = "(pre-main)";
double mhz = -1.0;

/*
 * Set the program name, first statement of code usually.
 */
void osd_set_progname(int argc __unused, char *const argv[])
{
	const char *cp;

	for (cp=progname=argv[0]; *cp; cp++)
		if (*cp == '/')
			progname = cp+1;
	/* for timing tests */
	mhz = get_mhz();
}

/*
 * Debugging.
 */
void __attribute__((format(printf,1,2)))
osd_info(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ".\n");
}

/*
 * Warning, non-fatal.
 */
void __attribute__((format(printf,1,2)))
osd_warning(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: Warning: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ".\n");
}

/*
 * Error.
 */
void __attribute__((format(printf,1,2)))
osd_error(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: Error: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ".\n");
}

/*
 * Error with the errno message.
 */
void __attribute__((format(printf,1,2)))
osd_error_errno(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: Error: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s.\n", strerror(errno));
}

/*
 * Errno with the message corresponding to this explict -errno value.
 * It should be negative.
 */
void __attribute__((format(printf,2,3)))
osd_error_xerrno(int errnum, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: Error: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s.\n", strerror(-errnum));
}

/*
 * Error, fatal with the errno message.
 */
void __attribute__((format(printf,1,2)))
osd_error_fatal(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: Error: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s.\n", strerror(errno));
	exit(1);
}

/*
 * Error-checking malloc.
 */
void * __attribute__((malloc))
Malloc(size_t n)
{
	void *x = NULL;

	if (n == 0) {
		osd_error("%s: called on zero bytes", __func__);
	} else {
		x = malloc(n);
		if (!x)
			osd_error("%s: couldn't get %lu bytes", __func__, 
				  (unsigned long) n);
	}
	return x;
}

/*
 * Error-checking counted cleared memory.
 */
void * __attribute__((malloc))
Calloc(size_t nmemb, size_t n)
{
	void *x = NULL;

	if (n == 0)
		osd_error("%s: called on zero bytes", __func__);
	else {
		x = calloc(nmemb, n);
		if (!x)
			osd_error("%s: couldn't get %zu bytes", __func__, 
				  nmemb * n);
	}
	return x;
}

/*
 * For reading from a pipe, can't always get the full buf in one chunk.
 */
size_t osd_saferead(int fd, void *buf, size_t num)
{
	int i, offset = 0;
	int total = num;

	while (num > 0) {
		i = read(fd, (char *)buf + offset, num);
		if (i < 0)
			osd_error_errno("%s: read %zu bytes", __func__, num);
		if (i == 0) {
			if (offset == 0) 
				return 0; /* end of file on a block boundary */
			osd_error("%s: EOF, only %d of %d bytes", __func__, 
				  offset, total);
		}
		num -= i;
		offset += i;
	}
	return total;
}

size_t osd_safewrite(int fd, const void *buf, size_t num)
{
	int i, offset = 0;
	int total = num;

	while (num > 0) {
		i = write(fd, (const char *)buf + offset, num);
		if (i < 0)
			osd_error_errno("%s: write %zu bytes", __func__, num);
		if (i == 0)
			osd_error("%s: EOF, only %d of %d bytes", __func__,
				  offset, total);
		num -= i;
		offset += i;
	}
	return total;
}

/*
 * Debugging.
 */
void osd_hexdump(const uint8_t *d, size_t len)
{
	size_t offset = 0;

	while (offset < len) {
		unsigned int i, range;

		range = 8;
		if (range > len-offset)
			range = len-offset;
		printf("%4zx:", offset);
		for (i=0; i<range; i++)
			printf(" %02x", d[offset+i]);
		printf("\n");
		offset += range;
	}
}

/* endian functions */
static uint32_t swab32(uint32_t d)
{
	return  (d & (uint32_t) 0x000000ffUL) << 24 |
		(d & (uint32_t) 0x0000ff00UL) << 8  |
		(d & (uint32_t) 0x00ff0000UL) >> 8  |
		(d & (uint32_t) 0xff000000UL) >> 24;
}

/*
 * Things are not aligned in the current osd2r00, but they probably
 * will be soon.  Assume 4-byte alignment though.
 */
uint64_t ntohll_le(const uint8_t *d)
{
	uint32_t d0 = swab32(*(const uint32_t *) d);
	uint32_t d1 = swab32(*(const uint32_t *) (d+4));

	return (uint64_t) d0 << 32 | d1;
}

uint32_t ntohl_le(const uint8_t *d)
{
	return swab32(*(const uint32_t *) d);
}

uint16_t ntohs_le(const uint8_t *d)
{
	uint16_t x = *(const uint16_t *) d;

	return (x & (uint16_t) 0x00ffU) << 8 |
		(x & (uint16_t) 0xff00U) >> 8;
}

void set_htonll_le(uint8_t *x, uint64_t val)
{
	uint32_t *xw = (uint32_t *) x;

	xw[0] = swab32((val & (uint64_t) 0xffffffff00000000ULL) >> 32);
	xw[1] = swab32((val & (uint64_t) 0x00000000ffffffffULL));
}

void set_htonl_le(uint8_t *x, uint32_t val)
{
	uint32_t *xw = (uint32_t *) x;

	*xw = swab32(val);
}

void set_htons_le(uint8_t *x, uint16_t val)
{
	uint16_t *xh = (uint16_t *) x;

	*xh = (val & (uint16_t) 0x00ffU) << 8 |
		(val & (uint16_t) 0xff00U) >> 8;
}

/*
 * Offset fields for attribute lists are floating point-ish.  Smallest
 * possible offset (other than 0) is 2^8 == 256.
 */
uint64_t ntohoffset_le(const uint8_t *d)
{
	const uint32_t mask = 0xf0000000UL;
	uint32_t base;
	uint8_t exponent;
	uint64_t x;

	base = ntohl_le(d);
	exponent = (base & mask) >> 28;

	x = (uint64_t) (base & ~mask) << (exponent + 8);
	return x;
}

/*
 * This assumes that val is already rounded.  Else it loses bits as
 * it converts, effectively truncating.  These generally try to use
 * the smallest possible exponent to accommodate the value.
 */
void set_htonoffset_le(uint8_t *x, uint64_t val)
{
	const uint64_t max_mantissa = 0x0fffffffULL;
	uint64_t start = val;
	uint32_t base;
	uint8_t exponent;

	exponent = 0;
	val >>= 8;
	while (val > max_mantissa) {
		++exponent;
		val >>= 1;
	}
	if (exponent > 15) {
		osd_error("%s: offset 0x%llx too big for format", __func__,
		          llu(start));
		memset(x, 0, 4);
		return;
	}
	base = val;
	base |= (uint32_t) exponent << 28;
	set_htonl_le(x, base);
}

/*
 * Find the next legal offset >= start.
 */
uint64_t next_offset(uint64_t start)
{
	const uint64_t max_mantissa = 0x0fffffffULL;
	uint64_t val;
	uint8_t exponent;

	exponent = 8;
	val = start;
	val >>= exponent;
	while (val > max_mantissa) {
		++exponent;
		val >>= 1;
	}
recheck:
	if (exponent > 23) {
		osd_error("%s: offset 0x%llx too big for format", __func__,
		          llu(start));
		return 0;
	}

	/*
	 * Now we have found floor(val * 2^exponent) for start, but this
	 * could be less than start.
	 */
	if (val << exponent < start) {
		val += 1;
		if (val & ~max_mantissa) {
			/* roll to the next exponent */
			++exponent;
			val >>= 1;
			goto recheck;
		}
	}
	return val << exponent;
}

#ifdef TEST_NEXT_OFFSET
int main(void)
{
	struct {
		uint64_t start;
		uint64_t want;
	} v[] = {
		{ 0x0000000000000000ULL, 0x0000000000000000ULL },
		{ 0x0000000000000001ULL, 0x0000000000000100ULL },
		{ 0x0000000000000101ULL, 0x0000000000000200ULL },
		{ 0x0000000fffffff00ULL, 0x0000000fffffff00ULL },
		{ 0x0000000fffffff01ULL, 0x0000001000000000ULL },
		{ 0x0000000000800001ULL, 0x0000000000800100ULL },
		{ 0x0007ffffff800000ULL, 0x0007ffffff800000ULL },
		{ 0x0007ffffff800001ULL, 0x0000000000000000ULL }, /* too big */
		{ 0xffffffffffffffffULL, 0x0000000000000000ULL }, /* too big */
	};
	int i;

	for (i=0; i<ARRAY_SIZE(v); i++) {
		char x[4];
		uint64_t out, conv;
		out = next_offset(v[i].start);
		set_htonoffset_le(x, out);
		conv = ntohoffset_le(x);
		printf("%016llx -> %016llx %016llx %016llx %s\n", v[i].start,
		       v[i].want, out, conv,
		       (out == v[i].want && out == conv) ? "ok" : "BAD");
	}
	return 0;
}
#endif

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

	if (N <= 1)
		return 0.;

	for (i = 0; i < N; i++)
		sd += (v[i] - mu)*(v[i] - mu);
	sd = sqrt(sd / (N-1));
	return sd;
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

