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
	sqlite3_stmt *isprsnt;  /* is object present */
	sqlite3_stmt *emptypid; /* is partition empty */
	sqlite3_stmt *gettype;  /* get type of the object */
	sqlite3_stmt *getoids;  /* get oids in a pid */
	sqlite3_stmt *getcids;  /* get cids in pid */
	sqlite3_stmt *getpids;  /* get pids in db */
};

static void sqfinalize(sqlite3 *db, sqlite3_stmt *stmt, const char *SQL)
{
	int ret = 0;

	if (SQL[0] != '\0')
		error_sql(db, "prepare of %s failed", SQL);
	
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		error_sql(db, "%s: finalize failed", __func__);
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

	sprintf(SQL, "SELECT COUNT(oid) FROM %s WHERE pid = ? AND oid = ?;",
		dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->isprsnt, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_isprsnt;

	sprintf(SQL, "SELECT COUNT(oid) FROM %s WHERE pid = ? AND oid != 0 "
		" LIMIT 1;", dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->emptypid, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_emptypid;

	sprintf(SQL, "SELECT type FROM %s WHERE pid = ? AND oid = ?;",
		dbc->obj->name);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->gettype, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_gettype;

	sprintf(SQL, "SELECT oid FROM %s WHERE pid = ? AND type = %u AND "
		" oid >= ?;", dbc->obj->name, USEROBJECT);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->getoids, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_getoids;

	sprintf(SQL, "SELECT oid FROM %s WHERE pid = ? AND type = %u AND "
		" oid >= ?;", dbc->obj->name, COLLECTION);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->getcids, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_getcids;

	sprintf(SQL, "SELECT pid FROM %s WHERE oid = %llu AND type = %u AND "
		" pid >= ?;", dbc->obj->name, llu(PARTITION_OID), PARTITION);
	ret = sqlite3_prepare(dbc->db, SQL, -1, &dbc->obj->getpids, NULL);
	if (ret != SQLITE_OK)
		goto out_finalize_getpids;

	ret = OSD_OK; /* success */
	goto out;

out_finalize_getpids:
	sqfinalize(dbc->db, dbc->obj->getpids, SQL);
	SQL[0] = '\0';
out_finalize_getcids:
	sqfinalize(dbc->db, dbc->obj->getcids, SQL);
	SQL[0] = '\0';
out_finalize_getoids:
	sqfinalize(dbc->db, dbc->obj->getoids, SQL);
	SQL[0] = '\0';
out_finalize_gettype:
	sqfinalize(dbc->db, dbc->obj->gettype, SQL);
	SQL[0] = '\0';
out_finalize_emptypid:
	sqfinalize(dbc->db, dbc->obj->emptypid, SQL);
	SQL[0] = '\0';
out_finalize_isprsnt:
	sqfinalize(dbc->db, dbc->obj->isprsnt, SQL);
	SQL[0] = '\0';
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
	sqlite3_finalize(dbc->obj->isprsnt);
	sqlite3_finalize(dbc->obj->emptypid);
	sqlite3_finalize(dbc->obj->gettype);
	sqlite3_finalize(dbc->obj->getoids);
	sqlite3_finalize(dbc->obj->getcids);
	sqlite3_finalize(dbc->obj->getpids);
	free(dbc->obj->name);
	free(dbc->obj);
	dbc->obj = NULL;

	return OSD_OK;
}


static inline int reset_stmt(struct db_context *dbc, sqlite3_stmt *stmt)
{
	int ret = sqlite3_reset(stmt);
	if (ret == SQLITE_OK) {
		return OSD_OK;
	} else if (ret == SQLITE_SCHEMA) {
		obj_finalize(dbc);
		ret = obj_initialize(dbc);
		if (ret == OSD_OK)
			return OSD_REPEAT;
	} 
	return OSD_ERROR;
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
		error_sql(dbc->db, "%s: stmt failed: ", func);
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
 * NOTE: If the object is not present, the function completes successfully.
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


static int exec_int_rtrvl_stmt(struct db_context *dbc, int *val)
{
	return 0;
}

static int exec_int64_rtrvl_stmt(struct db_context *dbc, int *val)
{
	return 0;
}

/*
 * return values
 * -EINVAL: invalid args
 * OSD_ERROR: some other sqlite error
 * OSD_OK: success
 * 	oid = next oid if pid has some oids
 * 	oid = 1 if pid was empty or absent. caller must assign correct oid.
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
 * -EINVAL: invalid args
 * OSD_ERROR: some other sqlite error
 * OSD_OK: success
 * 	pid = next pid if OSD has some pids
 * 	pid = 1 if pid not in db. caller must assign correct pid.
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


/* 
 * NOTE: type not in arg, since USEROBJECT and COLLECTION share namespace 
 * and (pid, oid) is unique.
 *
 * returns:
 * -EINVAL: invalid arg, ignore value of *present
 * OSD_ERROR: some other error, ignore value of *present
 * OSD_OK: success, *present set to the following:
 * 	0: object is absent
 * 	1: object is present
 */
int obj_ispresent(struct db_context *dbc, uint64_t pid, uint64_t oid, 
		  int *present)
{
	int ret = 0;
	*present = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->isprsnt) {
		ret = -EINVAL;
		goto out;
	}

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->obj->isprsnt, 1, pid);
	ret |= sqlite3_bind_int64(dbc->obj->isprsnt, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		goto out_reset;
	}

	while ((ret = sqlite3_step(dbc->obj->isprsnt)) == SQLITE_BUSY);
	if (ret == SQLITE_ROW) {
		*present = sqlite3_column_int(dbc->obj->isprsnt, 0);
	} else {
		error_sql(dbc->db, "%s: exec failed", __func__);
	}

out_reset:
	ret = sqlite3_reset(dbc->obj->isprsnt);
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
 * tests whether partition is empty. 
 *
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: in case of other errors, ignore value of *empty
 * OSD_OK: success, *empty set to:
 * 	==1: if partition is empty or absent or in case of sqlite error
 * 	==0: if partition is not empty
 */
int obj_isempty_pid(struct db_context *dbc, uint64_t pid, int *isempty)
{
	int ret = 0;
	*isempty = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->emptypid) {
		ret = -EINVAL;
		goto out;
	}

repeat:
	ret = sqlite3_bind_int64(dbc->obj->emptypid, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		goto out_reset;
	}

	while ((ret = sqlite3_step(dbc->obj->emptypid)) == SQLITE_BUSY);
	if (ret == SQLITE_ROW) {
		*isempty = (0 == sqlite3_column_int(dbc->obj->emptypid, 0));
	} else {
		error_sql(dbc->db, "%s: exec failed", __func__);
	}

out_reset:
	ret = sqlite3_reset(dbc->obj->emptypid);
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
 * return the type of the object
 *
 * returns:
 * -EINVAL: invalid arg, ignore value of obj_type
 * OSD_ERROR: any other error, ignore value of obj_type
 * OSD_OK: success in determining the type, either valid or invalid. 
 * 	obj_types set to the determined type.
 */
int obj_get_type(struct db_context *dbc, uint64_t pid, uint64_t oid, 
		 uint8_t *obj_type)
{
	int ret = 0;
	*obj_type = ILLEGAL_OBJ;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->gettype) {
		ret = -EINVAL;
		goto out;
	}

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->obj->gettype, 1, pid);
	ret |= sqlite3_bind_int64(dbc->obj->gettype, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		goto out_reset;
	}

	while ((ret = sqlite3_step(dbc->obj->gettype)) == SQLITE_BUSY);
	if (ret == SQLITE_ROW) {
		*obj_type = sqlite3_column_int(dbc->obj->gettype, 0);
	} else if (ret == SQLITE_DONE) {
		osd_debug("%s: (%llu %llu) doesn't exist", __func__, 
			  llu(pid), llu(oid));
	} else {
		error_sql(dbc->db, "%s: exec failed", __func__);
	}

out_reset:
	ret = sqlite3_reset(dbc->obj->gettype);
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


static int exec_id_rtrvl_stmt(struct db_context *dbc, sqlite3_stmt *stmt, 
			      int ret, const char *func, uint64_t alloc_len, 
			      uint8_t *outdata, uint64_t *used_outlen, 
			      uint64_t *add_len, uint64_t *cont_id)
{
	uint64_t len = 0;

	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", func);
		goto out_reset;
	}

	len = 0;
	*add_len = 0;
	*cont_id = 0;
	*used_outlen = 0;
	while (1) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_ROW) {
			if ((alloc_len - len) >= 8) {
				set_htonll(outdata, 
					   sqlite3_column_int64(stmt, 0));
				outdata += 8;
				len += 8;
			} else if (*cont_id == 0) {
				*cont_id = sqlite3_column_int64(stmt, 0);
			}
			*add_len += 8;
		} else if (ret == SQLITE_BUSY) {
			continue;
		} else {
			break;
		}
	}

out_reset:
	ret = sqlite3_reset(stmt);
	if (ret == SQLITE_OK) {
		*used_outlen = len;
		ret = OSD_OK;
	} else if (ret == SQLITE_SCHEMA) {
		obj_finalize(dbc);
		ret = obj_initialize(dbc);
		if (ret == OSD_OK)
			ret = OSD_REPEAT;
		else
			ret = OSD_ERROR;
	} else {
		error_sql(dbc->db, "%s: stmt failed:", func);
		ret = OSD_ERROR;
	}

	return ret;
}


/*
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success. oids copied into outdata, contid, used_outlen and 
 * 	add_len are set accordingly
 */
int obj_get_oids_in_pid(struct db_context *dbc, uint64_t pid, 
			uint64_t initial_oid, uint64_t alloc_len, 
			uint8_t *outdata, uint64_t *used_outlen, 
			uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->getoids)
		return -EINVAL;

repeat:
	ret = 0;
	stmt = dbc->obj->getoids;
	ret |= sqlite3_bind_int64(stmt, 1, pid);
	ret |= sqlite3_bind_int64(stmt, 2, initial_oid);
	ret = exec_id_rtrvl_stmt(dbc, stmt, ret, __func__, alloc_len,
				 outdata, used_outlen, add_len, cont_id);
	if (ret == OSD_REPEAT)
		goto repeat;

	return ret;
}


/*
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success. cids copied into outdata, contid, used_outlen and 
 * 	add_len are set accordingly
 */
int obj_get_cids_in_pid(struct db_context *dbc, uint64_t pid, 
			uint64_t initial_cid, uint64_t alloc_len, 
			uint8_t *outdata, uint64_t *used_outlen, 
			uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->getcids)
		return -EINVAL;

repeat:
	ret = 0;
	stmt = dbc->obj->getcids;
	ret |= sqlite3_bind_int64(stmt, 1, pid);
	ret |= sqlite3_bind_int64(stmt, 2, initial_cid);
	ret = exec_id_rtrvl_stmt(dbc, stmt, ret, __func__, alloc_len,
				 outdata, used_outlen, add_len, cont_id);
	if (ret == OSD_REPEAT)
		goto repeat;

	return ret;
}


/*
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success. cids copied into outdata, contid, used_outlen and 
 * 	add_len are set accordingly
 */
int obj_get_all_pids(struct db_context *dbc, uint64_t initial_pid, 
		     uint64_t alloc_len, uint8_t *outdata,
		     uint64_t *used_outlen, uint64_t *add_len,
		     uint64_t *cont_id)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->obj || !dbc->obj->getpids)
		return -EINVAL;

repeat:
	ret = sqlite3_bind_int64(dbc->obj->getpids, 1, initial_pid);
	ret = exec_id_rtrvl_stmt(dbc, dbc->obj->getpids, ret, __func__,
				 alloc_len, outdata, used_outlen, add_len, 
				 cont_id);
	if (ret == OSD_REPEAT)
		goto repeat;

	return ret;
}
