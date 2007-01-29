#ifndef __OSD_TYPES_H
#define __OSD_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#define ROOT_PID (0LLU)
#define ROOT_OID (0LLU)
#define PARTITION_PID_LB (0x10000LLU)
#define PARTITION_OID (0x0LLU)
#define COLLECTION_PID_LB (0x10000LLU)
#define USEROBJECT_PID_LB (0x10000LLU)
#define COLLECTION_OID_LB COLLECTION_PID_LB
#define USEROBJECT_OID_LB USEROBJECT_PID_LB


typedef enum {
	ROOT = 1,
	PARTITION,
	COLLECTION,
	USEROBJECT
} typeof_obj_t;

/*
 * See osd2r00 pg 22
 */
typedef enum {
	USER_PG       = 0,
	PARTITION_PG  = 0x30000000,
	COLLECTION_PG = 0x60000000,
	ROOT_PG       = 0x90000000,
	RESERVED_PG   = 0xC0000000,
	ANY_PG        = 0xF0000000
} attrpg_range_t;

struct object {
	uint64_t pid;
	uint64_t oid;
}; 

typedef struct attr {
	uint32_t page;
	uint32_t number;
	uint16_t len;
	void *val;
} attr_t;
typedef  attr_t list_entry_t;
#define ATTR_VAL_OFFSET (offsetof(attr_t , val))

/*
 * Things that go as attributes on the root page.
 * XXX: on second thought, don't stick these in the db, just return
 * them as needed programatically.  There's plenty of other variable
 * ones that can't live in the db (clock, #objects, capacity used).
 */
struct init_attr {
	uint32_t page;
	uint32_t number;
	const char *s;
};

#define MAXSQLEN (2048UL)
#define MAXNAMELEN (256UL)
#define MAXROOTLEN (200UL)

struct osd_device {
	char *root;
	sqlite3 *db;
};

#endif /* __OSD_TYPES_H */
