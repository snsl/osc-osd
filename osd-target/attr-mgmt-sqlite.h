#ifndef __ATTR_MGMT_SQLITE_H
#define __ATTR_MGMT_SQLITE_H

#include "obfs-types.h"

int attrdb_open(const char *path, osd_t *osd);
int attrdb_close(osd_t *osd);

int create_partition(osd_t *osd, uint64_t pid);

int attrdb_creat_object_attr_tab(void *dbh, uint64_t pid, uint64_t oid);
int attrdb_drop_object_attr_tab(void *dbh, uint64_t pid, uint64_t oid);
int attrdb_set_attr(void *dbh, uint64_t pid, uint64_t oid, uint32_t page,
		    uint32_t number, const void *val, uint16_t len);
int attrdb_get_attr(void *dbh, uint64_t pid, uint64_t  oid, uint32_t page,
		    uint32_t number, void *outbuf, uint16_t len);
int attrdb_get_attr_page(void *dbh, uint64_t pid, uint64_t oid, uint32_t page,
			 void *outbuf, uint16_t len);

#endif /* __ATTR_MGMT_SQLITE_H */
