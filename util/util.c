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
#include "util.h"

#define __unused __attribute__((unused))

/* global */
const char *progname = "(pre-main)";

/*
 * Set the program name, first statement of code usually.
 */
void
osd_set_progname(int argc __unused, char *const argv[])
{
    const char *cp;

    for (cp=progname=argv[0]; *cp; cp++)
	if (*cp == '/')
	    progname = cp+1;
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
 * XXX: later add first parameter "level".
 */
void __attribute__((format(printf,1,2)))
osd_debug(const char *fmt, ...)
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

    if (n == 0)
	osd_error("%s: called on zero bytes", __func__);
    else {
	x = malloc(n);
	if (!x)
	    osd_error("%s: couldn't get %lu bytes", __func__, (unsigned long) n);
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
	    osd_error("%s: couldn't get %zu bytes", __func__, nmemb * n);
    }
    return x;
}

/*
 * For reading from a pipe, can't always get the full buf in one chunk.
 */
size_t
osd_saferead(int fd, void *buf, size_t num)
{
    int i, offset = 0;
    int total = num;

    while (num > 0) {
	i = read(fd, (char *)buf + offset, num);
	if (i < 0)
	    osd_error_errno("%s: read %zu bytes", __func__, num);
	if (i == 0) {
	    if (offset == 0) return 0;  /* end of file on a block boundary */
	    osd_error("%s: EOF, only %d of %d bytes", __func__, offset, total);
	}
	num -= i;
	offset += i;
    }
    return total;
}

size_t
osd_safewrite(int fd, const void *buf, size_t num)
{
    int i, offset = 0;
    int total = num;

    while (num > 0) {
	i = write(fd, (const char *)buf + offset, num);
	if (i < 0)
	    osd_error_errno("%s: write %zu bytes", __func__, num);
	if (i == 0)
	    osd_error("%s: EOF, only %d of %d bytes", __func__, offset, total);
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
		printf("%4lx:", offset);
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

void set_htonoffset_le(uint8_t *x, uint64_t val)
{
	const uint32_t mask = 0xf0000000UL;
	uint32_t base;
	uint8_t exponent;

	exponent = 0;
	val >>= 8;
	while (val > ~mask) {
		++exponent;
		val >>= 1;
	}
	base = val;
	base |= (uint32_t) exponent << 28;
	set_htonl_le(x, base);
}

