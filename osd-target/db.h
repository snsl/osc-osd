#ifndef __DB_H
#define __DB_H

#include <sqlite3.h>

int db_open(const char *path, struct osd_device *osd);
int db_close(struct osd_device *osd);
int db_exec_pragma(struct osd_device *osd);
void error_sql(sqlite3 *db, const char *fmt, ...)
	__attribute__((format(printf,2,3))); 
int db_print_pragma(struct osd_device *osd);
int db_begin_txn(struct osd_device *osd);
int db_end_txn(struct osd_device *osd);

#endif /* __DB_H */
