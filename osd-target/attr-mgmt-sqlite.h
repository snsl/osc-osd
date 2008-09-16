#ifndef __ATTR_MGMT_SQLITE_H
#define __ATTR_MGMT_SQLITE_H

#include "obfs-types.h"

int attrdb_open(const char *path, osd_t *osd);
int attrdb_close(osd_t *osd); 
int attrdb_creat_object_attr_tab(void *dbh, object_id_t id, object_t type);
int attrdb_drop_object_attr_tab(void *dbh, object_id_t id);
int attrdb_set_attr(void *dbh, object_id_t id, attr_pgnum_t pg, 
		    attr_num_t num, attr_val_len_t len, void *val);
int attrdb_get_attr(void *dbh, object_id_t id, attr_pgnum_t pg, 
		    attr_num_t num, attr_val_len_t len, void *outbuf);
int attrdb_get_attr_page(void *dbh, object_id_t id, attr_pgnum_t pg, 
			 attr_val_len_t len, void *outbuf);

#endif /* __ATTR_MGMT_SQLITE_H */
