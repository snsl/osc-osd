#ifndef __OSD_TYPES_H
#define __OSD_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

struct object {
	uint64_t pid;
	uint64_t oid;
}; 

struct list_entry {
	uint32_t page;
	uint32_t number;
	uint16_t len;
	void *val;
} __attribute__((packed));

enum {
	LE_PAGE_OFF = offsetof(struct list_entry, page),
	LE_NUMBER_OFF = offsetof(struct list_entry, number),
	LE_LEN_OFF = offsetof(struct list_entry, len),
	LE_VAL_OFF = offsetof(struct list_entry, val)
};

struct multiobj_list_entry {
	uint64_t oid;
	uint32_t page;
	uint32_t number;
	uint16_t len;
	void *val;
} __attribute__((packed));

enum {
	MLE_OID_OFF = offsetof(struct multiobj_list_entry, oid),
	MLE_PAGE_OFF = offsetof(struct multiobj_list_entry, page),
	MLE_NUMBER_OFF = offsetof(struct multiobj_list_entry, number),
	MLE_LEN_OFF = offsetof(struct multiobj_list_entry, len),
	MLE_VAL_OFF = offsetof(struct multiobj_list_entry, val)
};

#define MIN_LIST_LEN (8) /* XXX: osd-errata */
#define LIST_LEN_UB (0xFFFEU)
#define NULL_ATTR_LEN (0xFFFFU) /* osd2r00 Sec 7.1.1 */
#define NULL_PAGE_LEN (0x00) /* osd2r00 Sec 7.1.2.25 */

/* osd2r00 Section 7.1.3.1 tab 127 */
enum {
	RTRV_ATTR_LIST = 0x01,
	RTRVD_SET_ATTR_LIST = 0x09,
	RTRVD_CREATE_ATTR_LIST = 0x0F
};

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
#define TIME_SZ (6) 

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
