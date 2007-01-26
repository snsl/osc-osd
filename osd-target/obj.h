#ifndef __OBJ_H
#define __OBJ_H

#include <sqlite3.h>

int obj_insert(sqlite3 *db, uint64_t pid, uint64_t oid);
int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid);

#endif /* __OBJ_H */
