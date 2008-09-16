#ifndef __ATTR_MGMT_SQLITE_H
#define __ATTR_MGMT_SQLITE_H

#include "obfs-types.h"

int attrdb_open(const char *path, osd_t *osd);
int attrdb_close(osd_t *osd); 
int attrdb_creat_object_attr_tab(void *dbh, object_id_t id, object_t type);
int attrdb_drop_object_attr_tab(void *dbh, object_id_t id);
int attrdb_set_attr(void *dbh, object_id_t id, uint32_t pg, 
		    uint32_t num, uint16_t len, const void *val);
int attrdb_get_attr(void *dbh, object_id_t id, uint32_t pg, 
		    uint32_t num, uint16_t len, void *outbuf);
int attrdb_get_attr_page(void *dbh, object_id_t id, uint32_t pg, 
			 uint16_t len, void *outbuf);

#endif /* __ATTR_MGMT_SQLITE_H */
