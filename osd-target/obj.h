#ifndef __OBJ_H
#define __OBJ_H

#include <sqlite3.h>

/*
 * Constants
 */

/*
CREATE TABLE obj (
	pid INTEGER,
	oid INTEGER,
	PRIMARY KEY (pid, oid)
);

*/

#define OBJ_PID_COLUMN 		1
#define OBJ_OID_COLUMN 		2


int obj_insert(sqlite3 *db, uint64_t pid, uint64_t oid);
int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid);

#endif /* __OBJ_H */
