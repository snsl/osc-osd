#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "osd-types.h"
#include "util/osd-defs.h"
#include "db.h"
#include "object-collection.h"
#include "util/util.h"
#include "list-entry.h"

/* 
 * object_collection table stores many-to-many relationship between
 * userobjects and collections. Using this table members of a collection and
 * collections to which an object belongs can be computed efficiently.
 */
int oc_insert_cid_oid(sqlite3 *db, uint64_t cid, uint64_t oid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "INSERT INTO object_collection VALUES (%llu, %llu);",
		llu(cid), llu(oid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

int oc_delete_cid(sqlite3 *db, uint64_t cid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM object_collection WHERE cid = %llu;", 
		llu(cid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

int oc_delete_oid(sqlite3 *db, uint64_t oid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM object_collection WHERE oid = %llu;", 
		llu(oid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

/*
 * Collection Attributes Page (CAP) of a userobject stores its membership in 
 * collections osd2r01 Sec 7.1.2.19.
 */
int oc_get_cap(sqlite3 *db, uint64_t oid, void *outbuf, uint64_t outlen,
	       uint8_t listfmt, uint32_t *used_outlen)
{
	return -1;
}


int oc_get_oids_in_cid(sqlite3 *db, uint64_t cid, void **buf)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	uint64_t cnt = 0;
	uint64_t cur_cnt = 0;
	uint64_t *oids = NULL;
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT COUNT(oid) FROM object_collection WHERE cid = "
		" %llu	UNION ALL SELECT oid FROM object_collection WHERE"
		" cid = %llu;", llu(cid), llu(cid));
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		goto out; /* no prepared stmt, no need to finalize */
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (oids == NULL) {
			cnt = sqlite3_column_int64(stmt, 0);
			oids = (uint64_t *)Malloc(cnt * sizeof(uint64_t));
			if (!oids) {
				ret = -ENOMEM;
				goto out_finalize;
			}
			continue;
		}
		oids[cur_cnt] = (uint64_t)sqlite3_column_int64(stmt, 0);
		cur_cnt++;
	}

	assert (cur_cnt == cnt);

	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	} 

	*buf = oids;
	ret = 0;

out_finalize:
	/* 'ret' must be preserved. */
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}
	
out:
	return ret;
}
