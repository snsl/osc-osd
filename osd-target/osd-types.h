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

struct osd_device {
	char *root;
	sqlite3 *db;
};

#endif /* __OSD_TYPES_H */
