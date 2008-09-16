#ifndef __DB_H
#define __DB_H

#include <sqlite3.h>

int db_open(const char *path, osd_t *osd);
int db_close(osd_t *osd);
int db_err_finalize(const char *errmsg, sqlite3_stmt *stmt, int ret);
int db_err_free(char *err, int ret);

#endif /* __DB_H */
