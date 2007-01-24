
#include <sqlite3.h>

void error(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error_fatal(const char *fmt, ...) 
	__attribute__((noreturn, format(printf,1,2)));
void error_errno(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error_sql(sqlite3 *db, const char *fmt, ...) __attribute__((format(printf,2,3)));
void debug(const char *fmt, ...) __attribute__((format(printf,1,2)));
void *Malloc(size_t n) __attribute__((malloc));
void *Calloc(size_t nmemb, size_t n) __attribute__((malloc));

#define ARRAY_SIZE(x) (int)(sizeof(x) / sizeof((x)[0]))

#if __WORDSIZE == 64
#define llu(x) ((unsigned long long) (x))
#else
#define llu(x) (x)
#endif

