/*
 * Declarations of util.c utilities.
 *
 * Copyright (C) 2000-7 Pete Wyckoff <pw@osc.edu>
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 */
extern const char *progname;
void osd_set_progname(int argc, char *const argv[]);
void osd_info(const char *fmt, ...) __attribute__((format(printf,1,2)));
void osd_debug(const char *fmt, ...) __attribute__((format(printf,1,2)));
void osd_warning(const char *fmt, ...) __attribute__((format(printf,1,2)));
void osd_error(const char *fmt, ...) __attribute__((format(printf,1,2)));
void osd_error_errno(const char *fmt, ...) __attribute__((format(printf,1,2)));
void osd_error_xerrno(int errnum, const char *fmt, ...) __attribute__((format(printf,2,3)));
void osd_error_fatal(const char *fmt, ...) __attribute__((format(printf,1,2)));
void *Malloc(size_t n) __attribute__((malloc));
void *Calloc(size_t nmemb, size_t n) __attribute__((malloc));
ssize_t osd_saferead(int fd, void *buf, size_t num);
ssize_t osd_safewrite(int fd, const void *buf, size_t num);
void osd_hexdump(const uint8_t *d, size_t len);

#define ARRAY_SIZE(x) (int)(sizeof(x) / sizeof((x)[0]))

#ifndef llu
#if __WORDSIZE == 64
#define llu(x) ((unsigned long long) (x))
#else
#define llu(x) (x)
#endif
#endif

/* endian covertors */
uint16_t ntohs_le(const uint8_t *d);
uint32_t ntohl_le(const uint8_t *d);
uint64_t ntohll_le(const uint8_t *d);
uint64_t ntohoffset_le(const uint8_t *d);
void set_htons_le(uint8_t *x, uint16_t val);
void set_htonl_le(uint8_t *x, uint32_t val);
void set_htonll_le(uint8_t *x, uint64_t val);
void set_htonoffset_le(uint8_t *x, uint64_t val);

/* some day deal with the big-endian versions */
#define     ntohs      ntohs_le
#define     ntohl      ntohl_le
#define     ntohll     ntohll_le
#define     ntohoffset ntohoffset_le
#define set_htons      set_htons_le
#define set_htonl      set_htonl_le
#define set_htonll     set_htonll_le
#define set_htonoffset set_htonoffset_le

