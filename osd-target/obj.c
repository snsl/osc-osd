#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include "osd-types.h"
#include "util/osd-defs.h"
#include "osd.h"
#include "util/util.h"
#include "obj.h"
#include "db.h"

/* init
close */
int obj_insert(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t type)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	int exists = 0;

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
		osd_debug("%s: object %llu %llu exists", __func__,
			  llu(pid), llu(oid));
		exists = 1;
		goto out_finalize;
	} 

out_finalize:
	/* 
	 * NOTE: sqlite3_finalize grabs the correct error code in case of
	 * failure. else it returns SQLITE_OK (0) . hence, value in 'ret' is
	 * not lost
	 */
	ret = sqlite3_finalize(stmt); 
	if (ret != SQLITE_OK) {
		if (!(ret == SQLITE_CONSTRAINT || exists))
			error_sql(db, "%s: finalize", __func__);
		ret = -EEXIST;
	}
	
out:
	return ret;
}

/*
 * on success returns 0. 
 * If the object is not present, the function completes successfully.
 */
int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM obj WHERE pid = %llu AND oid = %llu;",
		llu(pid), llu(oid));
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
 * oid = next oid if pid found in db
 * oid = 1 if pid not in db. caller must assign correct oid.
 */
int obj_get_nextoid(sqlite3 *db, uint64_t pid, uint64_t *oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT MAX (oid) FROM obj WHERE pid = ?");
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

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/* store next oid; if illegal (pid, oid), *oid will be 1 */
		*oid = sqlite3_column_int64(stmt, 0) + 1; 
	}
	if (ret != SQLITE_DONE)
		error_sql(db, "%s: max oid for pid %llu", __func__, llu(pid));

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		error_sql(db, "%s: finalize", __func__);
	
out:
	return ret;
}

/*
 * return values
 * < 0 : in case of error
 * 0 : on success
 * pid = next pid in db
 * pid = 1 if db has no pids. caller must assign correct pid.
 */
int obj_get_nextpid(sqlite3 *db, uint64_t *pid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT MAX (pid) FROM obj;");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/* store next pid; if illegal pid, *pid will be 1 */
		*pid = sqlite3_column_int64(stmt, 0) + 1; 
	}
	if (ret != SQLITE_DONE) 
		error_sql(db, "%s: max pid error", __func__);

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		error_sql(db, "%s: finalize", __func__);
	
out:
	return ret;
}

/* 
 * returns: 1 if present, 0 if absent, -1 incase of any errors
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
	if (ret != SQLITE_DONE)
		error_sql(db, "%s: max oid for pid %llu, oid %llu", __func__, 
			  llu(pid), llu(oid));

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: finalize", __func__);
		return -1; /* error, return -1 */
	}
	
out:
	return present;
}


/*
 * tests whether partition is empty. 
 * = 1: if partition is empty or absent or in case of sqlite error
 * = 0: if not empty
 */
int obj_pid_isempty(sqlite3 *db, uint64_t pid)
{
	int ret = 0;
	int count = -1;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT COUNT (*) FROM obj WHERE pid = ? AND oid != 0;");
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

	while((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}
	if (ret != SQLITE_DONE) 
		error_sql(db, "%s: count query failed pid %llu", __func__, 
			  llu(pid));

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) 
		error_sql(db, "%s: finalize", __func__);

out:
	return (count == 0);
}

int obj_get_type(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	uint8_t obj_type = ILLEGAL_OBJ;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT type FROM obj WHERE pid = ? AND oid = ?;");
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

	while((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		obj_type = sqlite3_column_int(stmt, 0);
	}
	if (ret != SQLITE_DONE)
		error_sql(db, "%s: count query failed pid %llu", __func__, 
			  llu(pid));

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) 
		error_sql(db, "%s: finalize", __func__);
out:
	return obj_type;

}

int obj_get_oids_in_pid(sqlite3 *db, uint64_t pid, uint64_t initial_oid,
			uint64_t alloc_len, uint8_t *outdata, 
			uint64_t *used_outlen, uint64_t *add_len, 
			uint64_t *cont_id)
{
	int ret = 0;
	uint64_t len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT oid FROM obj WHERE pid = %llu AND oid >= %llu "
		" AND type = %u", llu(pid), llu(initial_oid), USEROBJECT);
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}

	len = *used_outlen = 0;
	*add_len = 0;
	*cont_id = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((alloc_len - len) >= 8) {
			set_htonll(outdata, sqlite3_column_int64(stmt, 0));
			outdata += 8;
			len += 8;
		} else if (*cont_id == 0) {
			*cont_id = sqlite3_column_int64(stmt, 0);
		}
		*add_len += 8;
	}
	*used_outlen = len;

	if (ret != SQLITE_DONE)
		error_sql(db, "%s: query %s failed", __func__, SQL);

	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) 
		error_sql(db, "%s: finalize", __func__);

out:
	return ret;
}

int obj_get_all_pids(sqlite3 *db, uint64_t initial_oid, uint64_t alloc_len,
		     uint8_t *outdata, uint64_t *used_outlen, 
		     uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	uint64_t len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT pid FROM obj WHERE oid = %llu AND pid >= %llu "
		" AND type = %u", PARTITION_OID, llu(initial_oid), PARTITION);
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}

	len = *used_outlen = 0;
	*add_len = 0;
	*cont_id = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((alloc_len - len) >= 8) {
			set_htonll(outdata, sqlite3_column_int64(stmt, 0));
			outdata += 8;
			len += 8;
		} else if (*cont_id == 0) {
			*cont_id = sqlite3_column_int64(stmt, 0);
		}
		*add_len += 8;
	}
	*used_outlen = len;

	if (ret != SQLITE_DONE)
		error_sql(db, "%s: query %s failed", __func__, SQL);

	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) 
		error_sql(db, "%s: finalize", __func__);

out:
	return ret;
}
