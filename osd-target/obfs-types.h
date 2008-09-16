#ifndef __OBFS_TYPES_H
#define __OBFS_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t object_id_t;
typedef uint64_t object_len_t;
typedef uint64_t object_off_t;
typedef uint64_t partition_id_t;
typedef uint64_t usrobject_id_t;
typedef uint64_t collection_id_t;
typedef uint16_t num_of_usr_object_t;

typedef uint32_t attr_len_t;
typedef uint32_t attr_off_t;

typedef uint32_t list_len_t;
typedef uint32_t list_off_t;
typedef uint32_t list_alloc_len_t;

typedef enum {
	ROOT = 1,
	PARTITION,
	COLLECTION,
	USEROBJECT
} object_t;

typedef enum {
	USER_PG = 0,
	PARTITION_PG = 0x30000000,
	COLLECTION_PG = 0x60000000,
	ROOT_PG = 0x90000000,
	RESERVED_PG = 0xC0000000
} attrpg_range_t;

typedef struct attr {
	uint32_t pgnum;
	uint32_t num;
	uint16_t len;
	void *val;
} attr_t;
typedef  attr_t list_entry_t;
#define ATTR_T_DSZ (offsetof(attr_t , val))

#define MAXSQLEN (2048UL)
#define MAXNAMELEN (256UL)
#define MAXROOTLEN (200UL)

typedef struct osd {
	char *root;
	void *db;
} osd_t;

#endif /* __OBFS_TYPES_H */
