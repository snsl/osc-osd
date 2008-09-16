
#include <sqlite3.h>

void error(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error_errno(const char *fmt, ...) __attribute__((format(printf,1,2)));
void error_sql(sqlite3 *db, const char *fmt, ...) __attribute__((format(printf,2,3)));
void debug(const char *fmt, ...) __attribute__((format(printf,1,2)));
void *Malloc(size_t n) __attribute__((malloc));

#define ARRAY_SIZE(x) (int)(sizeof(x) / sizeof((x)[0]))
