
#include <sqlite3.h>

void error_sql(sqlite3 *db, const char *fmt, ...) __attribute__((format(printf,2,3)));
