#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

#include "util.h"

void __attribute__((format(printf,1,2)))
error(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "osd: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
}

void __attribute__((noreturn, format(printf,1,2)))
error_fatal(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "osd: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
    exit(1);
}

void __attribute__((format(printf,1,2)))
error_errno(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "osd: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %m.\n");
}

void __attribute__((format(printf,2,3)))
error_sql(sqlite3 *db, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "osd: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s.\n", sqlite3_errmsg(db));
}

void __attribute__((format(printf,1,2)))
debug(const char *fmt, ...)
{
    va_list ap;

    /* fprintf(stderr, "obfs: "); */
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
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
    else {
	x = malloc(n);
	if (!x)
	    error("%s: couldn't get %zu bytes", __func__, n);
    }
    return x;
}

void * __attribute__((malloc))
Calloc(size_t nmemb, size_t n)
{
    void *x;

    if (n == 0)
	error("%s: called on zero bytes", __func__);
    else {
	x = calloc(nmemb, n);
	if (!x)
	    error("%s: couldn't get %zu bytes", __func__, n);
    }
    return x;
}
