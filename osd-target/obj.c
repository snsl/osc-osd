#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include "osd-types.h"
#include "osd.h"
#include "util.h"
#include "util/util.h"
#include "obj.h"
#include "db.h"

int obj_insert(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t type)
{
	int ret;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "INSERT INTO obj VALUES(?, ?, ?);");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 3, type);
	if (ret) {
		error_sql(db, "%s: bind type", __func__);
		goto out_finalize;
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		ret = sqlite3_reset(stmt); /* get real error code */
		if (ret == SQLITE_CONSTRAINT)
			ret = -EEXIST;
		error_sql(db, "%s: object %lu %lu exists!", __func__, pid, oid);
		goto out_finalize;
	} 

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: finalize", __func__);
		goto out;
	}
	
out:
	return ret;
}

int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM obj WHERE pid = %lu AND oid = %lu;",
		pid, oid);
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		sqlite3_free(err);
		return ret;
	}

	return 0;
}

/*
 * return values
 * < 0 : in case of error
 * 0 : on success
 * oid = next oid if pid and type found in db
 * oid = 1 if pid/type not in db. caller must assign correct oid.
 */
int obj_get_nextoid(sqlite3 *db, uint64_t pid, uint32_t type, uint64_t *oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT MAX (oid) FROM obj WHERE pid = ? AND TYPE = ?;");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 2, type);
	if (ret) {
		error_sql(db, "%s: bind type", __func__);
		goto out_finalize;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/* store next oid; if new (pid, oid) *oid will be 1 */
		*oid = sqlite3_column_int64(stmt, 0) + 1; 
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: max oid for pid %llu, type %u", __func__, 
			  llu(pid), type);
		goto out_finalize;
	} 

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: finalize", __func__);
		goto out;
	}
	
out:
	return ret;
}

/* 
 * returns: 1 if present, 0 otherwise.
 * NOTE: type not in arg, since USEROBJECT and COLLECTION share namespace 
 * and (pid, oid) is unique.
 */
int obj_ispresent(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	int present = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT COUNT (*) FROM obj WHERE pid = ? AND oid = ?;");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		present = sqlite3_column_int(stmt, 0);
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: max oid for pid %llu, oid %llu", __func__, 
			  llu(pid), llu(oid));
		goto out_finalize;
	} 

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: finalize", __func__);
		goto out;
	}
	
out:
	return present;
}
