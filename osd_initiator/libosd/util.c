/*
 * Copyright (C) 2000-6 Pete Wyckoff <pw@osc.edu>
 */
#include <stdio.h>
#include <stdlib.h>
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
set_progname(int argc __unused, char *const argv[])
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
info(const char *fmt, ...)
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
warning(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: Warning: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
}

/*
 * Error, fatal.
 */
void __attribute__((noreturn,format(printf,1,2)))
error(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: Error: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
    exit(1);
}

/*
 * Error, fatal, with the errno message.
 */
void __attribute__((noreturn,format(printf,1,2)))
error_errno(const char *fmt, ...)
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
    void *x;

    if (n == 0)
	error("%s: called on zero bytes", __func__);
    x = malloc(n);
    if (!x)
	error("%s: couldn't get %lu bytes", __func__, (unsigned long) n);
    return x;
}

/*
 * For reading from a pipe, can't always get the full buf in one chunk.
 */
ssize_t
saferead(int fd, void *buf, size_t num)
{
    int i, offset = 0;
    int total = num;

    while (num > 0) {
	i = read(fd, (char *)buf + offset, num);
	if (i < 0)
	    error_errno("%s: read %zu bytes", __func__, num);
	if (i == 0) {
	    if (offset == 0) return 0;  /* end of file on a block boundary */
	    error("%s: EOF, only %d of %d bytes", __func__, offset, total);
	}
	num -= i;
	offset += i;
    }
    return total;
}

ssize_t
safewrite(int fd, const void *buf, size_t num)
{
    int i, offset = 0;
    int total = num;

    while (num > 0) {
	i = write(fd, (const char *)buf + offset, num);
	if (i < 0)
	    error_errno("%s: write %zu bytes", __func__, num);
	if (i == 0)
	    error("%s: EOF, only %d of %d bytes", __func__, offset, total);
	num -= i;
	offset += i;
    }
    return total;
}

