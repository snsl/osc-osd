#ifndef __DB_H
#define __DB_H

#include <sqlite3.h>

int db_open(const char *path, struct osd_device *osd);
int db_close(struct osd_device *osd);
void error_sql(sqlite3 *db, const char *fmt, ...)
	__attribute__((format(printf,2,3))); 

#endif /* __DB_H */
