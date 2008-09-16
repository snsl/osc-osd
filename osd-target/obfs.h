#ifndef __OBFS_H
#define __OBFS_H

#include <obfs-types.h>

int osd_format(osd_size_t cap);

int osd_open();

int osd_close();

int osd_create(partition_id_t pid, usrobject_id_t uid, 
	       num_of_usr_object_t num);

int osd_create_partition(partition_id_t pid);

int osd_create_collection(partition_id_t pid, collection_id_t cid);

int osd_remove(partition_id_t pid, usrobject_id_t uid);

int osd_remove_partition(partition_id_t pid);

int osd_remove_collection(partition_id_t pid, collection_id_t cid);

int osd_get_attr_pg(attr_pgnum_t pg, attr_len_t alloc_len, 
		    attr_off_t ret_attr_off, void *dout_buf);

int osd_set_attr(attr_pgnum_t pg, attr_num_t attr_num, attr_len_t attr_len,
		 attr_off_t off, void *din_buf);

int osd_get_attr_list(list_len_t len, list_off_t off, 
		      list_alloc_len_t alloc_len, void *dout_buf);

int osd_set_attr_list(list_off_t ret_off, list_len_t len, list_off_t off);

int osd_read(partition_id_t pid, usrobject_id_t uid, object_len_t len,
	     object_off_t start);

int osd_write(partition_id_t pid, usrobject_id_t uid, object_len_t len,
	      object_off_t start);

#endif /* __OBFS_H */
