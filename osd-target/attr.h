#ifndef __ATTR_MGMT_SQLITE_H
#define __ATTR_MGMT_SQLITE_H

#include <sqlite3.h>
#include "osd-types.h"

/*
 * Constants
 */

/*
CREATE TABLE attr (
	pid INTEGER,
	oid INTEGER,
	page INTEGER,
	number INTEGER,
	value BLOB,
	PRIMARY KEY (pid, oid, page, number)
);
*/

#define ATTR_PID_COLUMN 	1
#define ATTR_OID_COLUMN 	2
#define ATTR_PAGE_COLUMN 	3
#define ATTR_NUMBER_COLUMN 	4
#define ATTR_VALUE_COLUMN 	5

int attr_set_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, const void *val, uint16_t len);
int attr_delete_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		     uint32_t number);
int attr_delete_all(sqlite3 *db, uint64_t pid, uint64_t oid);
int attr_set_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page,
		  uint32_t number, const void *val, uint16_t len);
int attr_get_attr(sqlite3 *db, uint64_t pid, uint64_t  oid, uint32_t page,
		  uint32_t number, void *outbuf, uint16_t len);
int attr_get_attr_page(sqlite3 *db, uint64_t pid, uint64_t oid, 
		       uint32_t page, void *outbuf, uint16_t len);

#endif /* __ATTR_MGMT_SQLITE_H */
