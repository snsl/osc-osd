#ifndef __OSD_TYPES_H
#define __OSD_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

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
#define BLOCK_SZ (512UL)

struct id_cache {
	uint64_t next_pid; /* next free pid */
	uint64_t cur_pid;  /* last pid referenced */
	uint64_t next_oid; /* next free oid within partition (cur_pid) */
};

struct __attribute__((packed)) cur_cmd_attr_pg {
	uint16_t cdb_srvc_act; /* current cmd  */
	uint8_t ricv[20]; /* response integrity check value */
	uint8_t obj_type;
	uint8_t reserved[3];
	uint64_t pid;
	uint64_t oid;
	uint64_t append_off;
};

struct osd_device {
	char *root;
	sqlite3 *db;
	struct cur_cmd_attr_pg ccap;
};

#endif /* __OSD_TYPES_H */
