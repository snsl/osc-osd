#ifndef __OBFS_TYPES_H
#define __OBFS_TYPES_H

#include <stdint.h>

typedef uint64_t partition_id_t;
typedef uint64_t usrobject_id_t;
typedef uint64_t collection_id_t;
typedef uint64_t object_id_t;
typedef uint16_t num_of_usr_object_t;
typedef uint64_t object_len_t;
typedef uint64_t object_off_t;


typedef uint32_t attr_pgnum_t;
typedef uint32_t attr_num_t;
typedef uint32_t attr_len_t;
typedef uint32_t attr_off_t;
typedef uint16_t attr_val_len_t;

typedef uint32_t list_len_t;
typedef uint32_t list_off_t;
typedef uint32_t list_alloc_len_t;

typedef struct list_entry_t {
	attr_pgnum_t pgnum;
	attr_num_t num;
	attr_val_len_t len;
	void *val;
} list_entry_t;

#endif /* __OBFS_TYPES_H */
