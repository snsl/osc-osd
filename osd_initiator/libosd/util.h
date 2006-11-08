/*
 * Declarations of util.c utilities.
 *
 * Copyright (C) 2000-6 Pete Wyckoff <pw@osc.edu>
 */
extern const char *progname;
extern void set_progname(int argc, char *const argv[]);
extern void info(const char *fmt, ...) __attribute__((format(printf,1,2)));
extern void warning(const char *fmt, ...) __attribute__((format(printf,1,2)));
extern void error(const char *fmt, ...)
  __attribute__((noreturn,format(printf,1,2)));
extern void error_errno(const char *fmt, ...)
  __attribute__((noreturn,format(printf,1,2)));
extern void *Malloc(size_t n) __attribute__((malloc));
extern ssize_t saferead(int fd, void *buf, size_t num);
extern ssize_t safewrite(int fd, const void *buf, size_t num);

