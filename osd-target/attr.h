#ifndef __ATTR_H
#define __ATTR_H

#include <sqlite3.h>
#include "osd-types.h"

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

#endif /* __ATTR_H */
