#ifndef __OBJECT_COLLECTION_H
#define __OBJECT_COLLECTION_H

#include <sqlite3.h>
#include "osd-types.h"

int oc_insert_cid_oid(sqlite3 *db, uint64_t cid, uint64_t oid);

int oc_delete_cid(sqlite3 *db, uint64_t cid);

int oc_delete_oid(sqlite3 *db, uint64_t oid);

int oc_get_cap(sqlite3 *db, uint64_t oid, void *outbuf, uint64_t outlen,
	       uint8_t listfmt, uint32_t *used_outlen);

int oc_get_oids_in_cid(sqlite3 *db, uint64_t cid, void **buf);

#endif /* __OBJECT_COLLECTION_H */
