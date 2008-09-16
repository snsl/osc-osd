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

#if 0
/*
 * obj table tracks the presence of objects in the OSD
 */
static const char *obj_tab_name = "obj";
struct obj_tab {
	char *name;             /* name of the table */
	sqlite3_stmt *insert;   /* insert a row */
	sqlite3_stmt *delete;   /* delete a row */
	sqlite3_stmt *delpid;   /* delete all rows for pid */
	sqlite3_stmt *nextoid;  /* get next oid */
	sqlite3_stmt *nextpid;  /* get next pid */
};

static void sqfinalize(sqlite3 *db, sqlite3_stmt *stmt, const char *SQL)
{
	int ret = 0;

	if (SQL[0] != '\0')
		error_sql(db, "prepare of %s failed", SQL);
	
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		error_sql(db, "%s: insert finalize failed", __func__);
}

/*
 * returns:
 * -ENOMEM: out of memory
 * -EINVAL: invalid args
 * -EIO: if any prepare statement fails
 *  OSD_OK: success
 */
int obj_initialize(struct db_context *dbc)
{
	int ret = 0;
	int sqlret = 0;
	char SQL[MAXSQLEN];

	if (dbc == NULL || dbc->db == NULL) {
		ret = -EINVAL;
		goto out;
	}

	if (dbc->obj != NULL) {
		if (strcmp(dbc->obj->name, obj_tab_name) != 0) {
			ret = -EINVAL;
			goto out;
		} else {
			obj_finalize(dbc);
		}
	}

	dbc->obj = Malloc(sizeof(*dbc->obj));
	if (!dbc->obj) {
		ret = -ENOMEM;
		goto out;
	}
	
	dbc->obj->name = strdup(obj_tab_name); 
	if (!dbc->obj->name) {
		ret = -ENOMEM;
		goto out;
	}

	sprintf(SQL, "INSERT INTO %s VALUES (?, ?, ?);", dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->insert, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_insert;

	sprintf(SQL, "DELETE FROM %s WHERE pid = ? AND oid = ?;", 
		dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->delete, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_delete;

	sprintf(SQL, "DELETE FROM %s WHERE pid = ?;", dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->delpid, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_delpid;

	sprintf(SQL, "SELECT MAX(oid) FROM %s WHERE pid = ?;", 
		dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->nextoid, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_nextoid;

	sprintf(SQL, "SELECT MAX(pid) FROM %s;", dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->nextpid, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_nextpid;

	ret = OSD_OK; /* success */
	goto out;

out_finalize_nextpid:
	sqfinalize(dbc->db, dbc->obj->nextpid, SQL);
	SQL[0] = '\0';
out_finalize_nextoid:
	sqfinalize(dbc->db, dbc->obj->nextoid, SQL);
	SQL[0] = '\0';
out_finalize_delpid:
	sqfinalize(dbc->db, dbc->obj->delpid, SQL);
	SQL[0] = '\0';
out_finalize_delete:
	sqfinalize(dbc->db, dbc->obj->delete, SQL);
	SQL[0] = '\0';
out_finalize_insert:
	sqfinalize(dbc->db, dbc->obj->insert, SQL);
	ret = -EIO;
out:
	return ret;
}


int obj_finalize(struct db_context *dbc)
{
	if (!dbc)
		return OSD_ERROR;

	/* finalize statements; ignore return values */
	sqlite3_finalize(dbc->obj->insert);
	sqlite3_finalize(dbc->obj->delete);
	sqlite3_finalize(dbc->obj->delpid);
	sqlite3_finalize(dbc->obj->nextoid);
	sqlite3_finalize(dbc->obj->nextpid);
	free(dbc->obj->name);
	free(dbc->obj);
	dbc->obj = NULL;

	return OSD_OK;
}

/*
 * exec_dms: executes prior prepared and bound data manipulation statement.
 * Only SQL statements with 'insert' or 'delete' may call this helper
 * function.
 *
 * returns:
 * OSD_ERROR: in case of error
 * OSD_OK: on success
 * OSD_REPEAT: in case of sqlite_schema error, statements are prepared again,
 * 	hence values need to be bound again.
 */
static int exec_dms(struct db_context *dbc, sqlite3_stmt *stmt, int ret, 
		    const char *func)
{
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", func);
		goto out_reset;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_BUSY);
	if (ret != SQLITE_DONE) {
		error_sql(dbc->db, "%s: exec failed", func);
	}

out_reset:
	ret = sqlite3_reset(stmt);
	if (ret == SQLITE_OK) {
		return OSD_OK;
	} else if (ret == SQLITE_SCHEMA) {
		if (obj_finalize(dbc) != OSD_OK)
			return OSD_ERROR;
		if (obj_initialize(dbc) != OSD_OK)
			return OSD_ERROR;
		return OSD_REPEAT;
	} else {
		return OSD_ERROR;
	}
}


/*
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success
 */
int obj_insert(struct db_context *dbc, uint64_t pid, uint64_t oid, 
	       uint32_t type)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->insert) 
		return -EINVAL;

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->obj->insert, 1, pid);
	ret |= sqlite3_bind_int64(dbc->obj->insert, 2, oid);
	ret |= sqlite3_bind_int(dbc->obj->insert, 3, type);
	ret = exec_dms(dbc, dbc->obj->insert, ret, __func__);
	if (ret == OSD_REPEAT)
		goto repeat;

	return ret;
}

/*
 * If the object is not present, the function completes successfully.
 *
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success
 */
int obj_delete(struct db_context *dbc, uint64_t pid, uint64_t oid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->delete) 
		return -EINVAL;

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->obj->delete, 1, pid);
	ret |= sqlite3_bind_int64(dbc->obj->delete, 2, oid);
	ret = exec_dms(dbc, dbc->obj->delete, ret, __func__);
	if (ret == OSD_REPEAT)
		goto repeat;

	return ret;
}


/*
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success
 */
int obj_delete_pid(struct db_context *dbc, uint64_t pid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->delpid) 
		return -EINVAL;

repeat:
	ret = sqlite3_bind_int64(dbc->obj->delpid, 1, pid);
	ret = exec_dms(dbc, dbc->obj->delpid, ret, __func__);
	if (ret == OSD_REPEAT)
		goto repeat;

	return ret;
}

/*
 * return values
 * -EINVAL: invalid args
 * OSD_ERROR: some other sqlite error
 * OSD_OK: success
 * 	oid = next oid if pid found in db
 * 	oid = 1 if pid not in db. caller must assign correct oid.
 */
int obj_get_nextoid(struct db_context *dbc, uint64_t pid, uint64_t *oid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->nextoid) {
		ret = -EINVAL;
		goto out;
	}

repeat:
	ret = sqlite3_bind_int64(dbc->obj->nextoid, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		goto out_reset;
	}

	while ((ret = sqlite3_step(dbc->obj->nextoid)) == SQLITE_BUSY);
	if (ret == SQLITE_ROW) {
		*oid = sqlite3_column_int64(dbc->obj->nextoid, 0) + 1;
	} else {
		error_sql(dbc->db, "%s:exec failed", __func__);
	}

out_reset:
	ret = sqlite3_reset(dbc->obj->nextoid);
	if (ret == SQLITE_OK) {
		ret = OSD_OK;
	} else if (ret == SQLITE_SCHEMA) {
		obj_finalize(dbc);
		ret = obj_initialize(dbc);
		if (ret == OSD_OK)
			goto repeat;
		else
			ret = OSD_ERROR;
	} else {
		ret = OSD_ERROR;
	}
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
int obj_get_nextpid(struct db_context *dbc, uint64_t *pid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->nextpid) {
		ret = -EINVAL;
		goto out;
	}

repeat:
	while ((ret = sqlite3_step(dbc->obj->nextpid)) == SQLITE_BUSY);
	if (ret == SQLITE_ROW) {
		*pid = sqlite3_column_int64(dbc->obj->nextpid, 0) + 1;
	} else {
		error_sql(dbc->db, "%s:exec failed", __func__);
	}

out_reset:
	ret = sqlite3_reset(dbc->obj->nextpid);
	if (ret == SQLITE_OK) {
		ret = OSD_OK;
	} else if (ret == SQLITE_SCHEMA) {
		obj_finalize(dbc);
		ret = obj_initialize(dbc);
		if (ret == OSD_OK)
			goto repeat;
		else
			ret = OSD_ERROR;
	} else {
		ret = OSD_ERROR;
	}
out:
	return ret;
}

#endif

int obj_insert(struct db_context *dbc, uint64_t pid, uint64_t oid, uint32_t type)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	int exists = 0;
	sqlite3 *db = dbc->db;

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
int obj_delete(struct db_context *dbc, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;
	sqlite3 *db = dbc->db;

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

int obj_delete_pid(struct db_context *dbc, uint64_t pid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;
	sqlite3 *db = dbc->db;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM obj WHERE pid = %llu;", llu(pid));
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
int obj_get_nextoid(struct db_context *dbc, uint64_t pid, uint64_t *oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

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
int obj_get_nextpid(struct db_context *dbc, uint64_t *pid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

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
int obj_ispresent(struct db_context *dbc, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	int present = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

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
int obj_isempty_pid(struct db_context *dbc, uint64_t pid)
{
	int ret = 0;
	int count = -1;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

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

int obj_get_oids_in_pid(struct db_context *dbc, uint64_t pid, 
			uint64_t initial_oid, uint64_t alloc_len, 
			uint8_t *outdata, uint64_t *used_outlen, 
			uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	uint64_t len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

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

int obj_get_all_pids(struct db_context *dbc, uint64_t initial_oid, 
		     uint64_t alloc_len, uint8_t *outdata,
		     uint64_t *used_outlen, uint64_t *add_len,
		     uint64_t *cont_id)
{
	int ret = 0;
	uint64_t len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT pid FROM obj WHERE oid = %llu AND pid >= %llu "
		" AND type = %u;", PARTITION_OID, llu(initial_oid), 
		PARTITION);
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}

	len = 0;
	*add_len = 0;
	*cont_id = 0;
	*used_outlen = 0;
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

/*
 * returns:
 * -EINVAL: invalid arg
 * ==0: success, cids copied into outbuf, cont_id set if necessary
 */
int obj_get_cids_in_pid(struct db_context *dbc, uint64_t pid, 
			uint64_t initial_cid, uint64_t alloc_len, 
			uint8_t *outdata, uint64_t *used_outlen, 
			uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	uint64_t len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;		
	sqlite3 *db = dbc->db;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT oid FROM obj WHERE pid = %llu AND oid >= %llu "
		" AND type = %u", llu(pid), llu(initial_cid), COLLECTION);
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	
	len = 0;
	*add_len = 0;
	*cont_id = 0;
	*used_outlen = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((alloc_len - len) >= 8) {
			set_htonll(outdata, sqlite3_column_int64(stmt,0));
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
