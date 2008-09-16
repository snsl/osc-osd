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

struct coll_tab {
	const char *name;      /* name of the table */
	sqlite3_stmt *insert;  /* insert a row */
	sqlite3_stmt *delete;  /* delete a row */
};

/*
 * returns:
 * -ENOMEM: out of memory
 * -EINVAL: invalid args
 * -EIO: if any prepare statement fails
 *  OSD_OK: success
 */
int oc_initialize(struct db_context *dbc)
{
	int ret = 0;
	int sqlret = 0;
	char SQL[MAXSQLEN];

	if (dbc == NULL || dbc->db == NULL || dbc->coll != NULL) {
		ret = -EINVAL;
		goto out;
	}

	dbc->coll = Malloc(sizeof(*dbc->coll));
	if (!dbc->coll) {
		ret = -ENOMEM;
		goto out;
	}
	
	dbc->coll->name = "coll"; 

	sprintf(SQL, "INSERT INTO %s VALUES (?, ?, ?, ?);", dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->insert, NULL);
	if (sqlret != SQLITE_OK) {
		ret = -EIO;
		error_sql(dbc->db, "%s: prepare of %s failed", __func__,
			  SQL); 
		goto out;
	}

	sprintf(SQL, "DELETE FROM %s WHERE pid = ? AND cid = ? AND oid = ?;", 
		dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->delete, NULL);
	if (ret != SQLITE_OK) {
		ret = -EIO;
		error_sql(dbc->db, "%s: prepare of %s failed", __func__, 
			  SQL);
		goto out_finalize_insert;
	}

	return OSD_OK; /* success */

out_finalize_insert:
	sqlret = sqlite3_finalize(dbc->coll->insert);
	if (sqlret != SQLITE_OK) {
		ret = -EIO;
		error_sql(dbc->db, "%s: insert finalize failed", __func__);
		goto out;
	}
out:
	return ret;
}


int oc_finalize(struct db_context *dbc)
{

	/* finalize statements; ignore return values */
	sqlite3_finalize(dbc->coll->insert);
	sqlite3_finalize(dbc->coll->delete);
	free(dbc->coll);

	return OSD_OK;
}

const char *coll_getname(struct db_context *dbc)
{
	if (dbc == NULL || dbc->coll == NULL)
		return NULL;
	return dbc->coll->name;
}

/* 
 * object_collection table stores many-to-many relationship between
 * userobjects and collections. Using this table members of a collection and
 * collections to which an object belongs can be computed efficiently.
 *
 * @pid: partition id 
 * @cid: collection id
 * @oid: userobject id
 * @number: attribute number of cid in CAP of oid
 */
int oc_insert_row(sqlite3 *db, uint64_t pid, uint64_t cid, uint64_t oid, 
		  uint32_t number)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "INSERT INTO object_collection VALUES "
		" (%llu, %llu, %llu, %u);", llu(pid), llu(cid), llu(oid), 
		number);
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

int oc_delete_row(sqlite3 *db, uint64_t pid, uint64_t cid, uint64_t oid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM object_collection WHERE pid = %llu AND "
		" cid = %llu AND oid = %llu;", llu(pid), llu(cid), llu(oid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

int oc_delete_all_cid(sqlite3 *db, uint64_t pid, uint64_t cid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM object_collection WHERE pid = %llu AND "
		" cid = %llu;", llu(pid), llu(cid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

int oc_delete_all_oid(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM object_collection WHERE pid = %llu "
		"AND oid = %llu;", llu(pid), llu(oid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

/*
 * tests whether collection is empty. 
 * < 0: in case of error
 * = 1: if collection is empty or absent 
 * = 0: if not empty
 */
int oc_isempty_cid(sqlite3 *db, uint64_t pid, uint64_t cid)
{
	int ret = 0;
	int count = -1;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT COUNT (*) FROM object_collection WHERE "
		" pid = %llu AND cid = %llu LIMIT 1;", llu(pid), llu(cid));
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		ret = -EIO;
		error_sql(db, "%s: prepare", __func__);
		goto out_err;
	}

	while((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}
	if (ret != SQLITE_DONE)
		error_sql(db, "%s: query %s failed", __func__, SQL);

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: finalize", __func__);
		ret = -EIO;
		goto out_err;
	}

out:
	return (count == 0);

out_err:
	return ret;
}

int oc_get_cid(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t number, 
	       uint64_t *cid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT cid FROM object_collection WHERE pid = %llu "
		" AND oid = %llu AND number = %u;", llu(pid), llu(oid), 
		number);
	sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	while ((ret == sqlite3_step(stmt)) == SQLITE_ROW) {
		*cid = sqlite3_column_int64(stmt, 0);
	}
	if (ret != SQLITE_DONE)
		error_sql(db, "%s: query %s failed", __func__, SQL);

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) 
		error_sql(db, "%s: finalize", __func__);

out:
	return ret;
}

/*
 * Collection Attributes Page (CAP) of a userobject stores its membership in 
 * collections osd2r01 Sec 7.1.2.19.
 */
int oc_get_cap(sqlite3 *db, uint64_t pid, uint64_t oid, void *outbuf, 
	       uint64_t outlen, uint8_t listfmt, uint32_t *used_outlen)
{
	return -1;
}


/*
 * returns:
 * -EINVAL: invalid arg
 * ==0: success, oids copied into outbuf, cont_id set if necessary
 */
int oc_get_oids_in_cid(sqlite3 *db, uint64_t pid, uint64_t cid, 
		       uint64_t initial_oid, uint64_t alloc_len, 
		       uint8_t *outdata, uint64_t *used_outlen,
		       uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	uint64_t len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;		

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT oid FROM object_collection WHERE pid = %llu "
			"AND cid = %llu AND oid >= %llu", llu(pid), llu(cid),
			llu(initial_oid));
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

/*
 * returns:
 * -EINVAL: invalid arg
 * ==0: success, cids copied into outbuf, cont_id set if necessary
 */
int oc_get_cids_in_pid(sqlite3 *db, uint64_t pid, uint64_t initial_oid,
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

	sprintf(SQL, "SELECT cid FROM object_collection WHERE pid = %llu "
			"AND cid >= %llu", llu(pid),
			llu(initial_oid));
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
