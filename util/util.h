/*
 * Declarations of util.c utilities.
 *
 * Copyright (C) 2000-7 Pete Wyckoff <pw@osc.edu>
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 */
extern const char *progname;
void set_progname(int argc, char *const argv[]);
void info(const char *fmt, ...) __attribute__((format(printf,1,2)));
void debug(const char *fmt, ...) __attribute__((format(printf,1,2)));
void warning(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error_errno(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error_fatal(const char *fmt, ...) __attribute__((format(printf,1,2)));
void *Malloc(size_t n) __attribute__((malloc));
void *Calloc(size_t nmemb, size_t n) __attribute__((malloc));
ssize_t saferead(int fd, void *buf, size_t num);
ssize_t safewrite(int fd, const void *buf, size_t num);

#define ARRAY_SIZE(x) (int)(sizeof(x) / sizeof((x)[0]))

#if __WORDSIZE == 64
#define llu(x) ((unsigned long long) (x))
#else
#define llu(x) (x)
#endif

/* endian covertors */
uint32_t swab32(uint32_t d);
uint64_t ntohll_le(uint8_t *d);
uint32_t ntohl_le(uint8_t *d);
uint16_t ntohs_le(uint8_t *d);

/* some day deal with the big-endian versions */
#define ntohs ntohs_le
#define ntohl ntohl_le
#define ntohll ntohll_le