#ifndef __OBJ_H
#define __OBJ_H

#include <sqlite3.h>

int obj_insert(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t type);
int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid);
int obj_get_nextoid(sqlite3 *db, uint64_t pid, uint64_t *oid);
int obj_get_nextpid(sqlite3 *db, uint64_t *pid);
int obj_ispresent(sqlite3 *db, uint64_t pid, uint64_t oid);

#endif /* __OBJ_H */
