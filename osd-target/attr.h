#ifndef __ATTR_MGMT_SQLITE_H
#define __ATTR_MGMT_SQLITE_H

#include "osd-types.h"

int attr_insert_object(void *dbh, uint64_t pid, uint64_t oid);
int attr_delete_object(void *dbh, uint64_t pid, uint64_t oid);
int attr_set_attr(void *dbh, uint64_t pid, uint64_t oid, uint32_t page,
	          uint32_t number, const void *val, uint16_t len);
int attr_get_attr(void *dbh, uint64_t pid, uint64_t  oid, uint32_t page,
	          uint32_t number, void *outbuf, uint16_t len);
int attr_getall_page_attr(void *dbh, uint64_t pid, uint64_t oid, 
			    uint32_t page, void *outbuf, uint16_t len);

#endif /* __ATTR_MGMT_SQLITE_H */
