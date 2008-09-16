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
#include "coll.h"
#include "util/util.h"
#include "list-entry.h"

/*
 * coll table stores many-to-many relationship between userobjects and
 * collections. Using this table members of a collection and collections to
 * which an object belongs can be computed efficiently.
 */

static const char *coll_tab_name = "coll";
struct coll_tab {
	char *name;       /* name of the table */
	sqlite3_stmt *insert;   /* insert a row */
	sqlite3_stmt *delete;   /* delete a row */
	sqlite3_stmt *remcid;   /* remove collection cid */
	sqlite3_stmt *remoid;   /* remove oid from all collections */
	sqlite3_stmt *emptycid; /* is collection empty? */
	sqlite3_stmt *getcid;   /* get collection */
	sqlite3_stmt *getoids;  /* get objects in a collection */
};

static void sqfinalize(sqlite3 *db, sqlite3_stmt *stmt, const char *SQL)
{
	int ret = 0;

	if (SQL[0] != '\0')
		error_sql(db, "prepare of %s failed", SQL);
	
	ret = sqlite3_finalize(stmt);
	if (ret  != SQLITE_OK)
		error_sql(db, "%s: insert finalize failed", __func__);
}

/*
 * returns:
 * -ENOMEM: out of memory
 * -EINVAL: invalid args
 * -EIO: if any prepare statement fails
 *  OSD_OK: success
 */
int coll_initialize(struct db_context *dbc)
{
	int ret = 0;
	int sqlret = 0;
	char SQL[MAXSQLEN];

	if (dbc == NULL || dbc->db == NULL) {
		ret = -EINVAL;
		goto out;
	}

	if (dbc->coll != NULL) {
		if (strcmp(dbc->coll->name, coll_tab_name) != 0) {
			ret = -EINVAL;
			goto out;
		} else {
			coll_finalize(dbc);
		}
	}

	dbc->coll = Malloc(sizeof(*dbc->coll));
	if (!dbc->coll) {
		ret = -ENOMEM;
		goto out;
	}
	
	dbc->coll->name = strdup(coll_tab_name); 
	if (!dbc->coll->name) {
		ret = -ENOMEM;
		goto out;
	}

	sprintf(SQL, "INSERT INTO %s VALUES (?, ?, ?, ?);", dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->insert, NULL);
	if (sqlret != SQLITE_OK)
		goto out_finalize_insert;

	sprintf(SQL, "DELETE FROM %s WHERE pid = ? AND cid = ? AND oid = ?;", 
		dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->delete, NULL);
	if (sqlret != SQLITE_OK)
		goto out_finalize_delete;

	sprintf(SQL, "DELETE FROM %s WHERE pid = ? AND cid = ?;", 
		dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->remcid, NULL);
	if (sqlret != SQLITE_OK)
		goto out_finalize_remcid;

	sprintf(SQL, "DELETE FROM %s WHERE pid = ? AND oid = ?;", 
		dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->remoid, NULL);
	if (sqlret != SQLITE_OK)
		goto out_finalize_remoid;

	sprintf(SQL, "SELECT COUNT (*) FROM %s WHERE pid = ? AND cid = ? "
		" LIMIT 1;", dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->emptycid, NULL);
	if (sqlret != SQLITE_OK)
		goto out_finalize_emptycid;

	sprintf(SQL, "SELECT cid FROM %s WHERE pid = ? AND oid = ? AND "
		" number = ?;", dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->getcid, NULL);
	if (sqlret != SQLITE_OK)
		goto out_finalize_getcid;

	sprintf(SQL, "SELECT oid FROM %s WHERE pid = ? AND cid = ? AND "
		" oid >= ?;", dbc->coll->name);
	sqlret = sqlite3_prepare(dbc->db, SQL, strlen(SQL)+1, 
				 &dbc->coll->getoids, NULL);
	if (sqlret != SQLITE_OK)
		goto out_finalize_getoids;

	ret = OSD_OK; /* success */
	goto out;

out_finalize_getoids:
	sqfinalize(dbc->db, dbc->coll->getoids, SQL);
	SQL[0] = '\0';
out_finalize_getcid:
	sqfinalize(dbc->db, dbc->coll->getcid, SQL);
	SQL[0] = '\0';
out_finalize_emptycid:
	sqfinalize(dbc->db, dbc->coll->emptycid, SQL);
	SQL[0] = '\0';
out_finalize_remoid:
	sqfinalize(dbc->db, dbc->coll->remoid, SQL);
	SQL[0] = '\0';
out_finalize_remcid:
	sqfinalize(dbc->db, dbc->coll->remcid, SQL);
	SQL[0] = '\0';
out_finalize_delete:
	sqfinalize(dbc->db, dbc->coll->delete, SQL);
	SQL[0] = '\0';
out_finalize_insert:
	sqfinalize(dbc->db, dbc->coll->insert, SQL);
	ret = -EIO;
out:
	return ret;
}


int coll_finalize(struct db_context *dbc)
{
	if (!dbc)
		return OSD_ERROR;

	/* finalize statements; ignore return values */
	sqlite3_finalize(dbc->coll->insert);
	sqlite3_finalize(dbc->coll->delete);
	sqlite3_finalize(dbc->coll->remcid);
	sqlite3_finalize(dbc->coll->remoid);
	sqlite3_finalize(dbc->coll->emptycid);
	sqlite3_finalize(dbc->coll->getcid);
	sqlite3_finalize(dbc->coll->getoids);
	free(dbc->coll->name);
	free(dbc->coll);
	dbc->coll = NULL;

	return OSD_OK;
}


const char *coll_getname(struct db_context *dbc)
{
	if (dbc == NULL || dbc->coll == NULL)
		return NULL;
	return dbc->coll->name;
}

/*
 * dms: data manipulation statements. Only SQL statements with 'insert' or
 * 'delete' may call this helper function.
 *
 * returns:
 * OSD_ERROR: in case of errpr
 * OSD_OK: on success
 * OSD_REPEAT: in case of sqlite_schema error, statements are prepared again,
 * 	hence values need to be bound again.
 */
static int exec_dms(struct db_context *dbc, sqlite3_stmt *stmt, int ret, 
		    const char *func)
{
	int sqlret = 0;

	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", func);
		ret = OSD_ERROR;
		goto out_reset;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_BUSY);
	if (ret != SQLITE_DONE) {
		error_sql(dbc->db, "%s: failed", func);
		ret = OSD_ERROR;
		goto out_reset;
	}

	ret = OSD_OK;

out_reset:
	sqlret = sqlite3_reset(stmt);
	if (sqlret == SQLITE_SCHEMA) {
		coll_finalize(dbc);
		coll_initialize(dbc);
		return OSD_REPEAT;
	}

	return ret;
}

/* 
 * @pid: partition id 
 * @cid: collection id
 * @oid: userobject id
 * @number: attribute number of cid in CAP of oid
 *
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success
 */
int coll_insert(struct db_context *dbc, uint64_t pid, uint64_t cid,
		uint64_t oid, uint32_t number)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt;

	if (!dbc || !dbc->db || !dbc->coll || !dbc->coll->insert)
		return -EINVAL;

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->coll->insert, 1, pid);
	ret |= sqlite3_bind_int64(dbc->coll->insert, 2, cid);
	ret |= sqlite3_bind_int64(dbc->coll->insert, 3, oid);
	ret |= sqlite3_bind_int(dbc->coll->insert, 4, number);
	ret = exec_dms(dbc, dbc->coll->insert, ret, __func__);
	if (ret == OSD_REPEAT)
		goto repeat;

	return ret;

/* 	ret = 0;
	ret |= sqlite3_bind_int64(dbc->coll->insert, 1, pid);
	ret |= sqlite3_bind_int64(dbc->coll->insert, 2, cid);
	ret |= sqlite3_bind_int64(dbc->coll->insert, 3, oid);
	ret |= sqlite3_bind_int(dbc->coll->insert, 4, number);

	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
	}

	stmt = dbc->coll->insert;
	while ((ret = sqlite3_step(stmt)) == SQLITE_BUSY);
	if (ret != SQLITE_DONE) {
		error_sql(dbc->db, "%s: failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
	}

	ret = OSD_OK;

out_reset:
	sqlite3_reset(stmt);
	return ret; */
/* 	osd_debug("%s: sqlite3_reset %d", __func__, sqlite3_reset(stmt)); */
}


/* 
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: some other error
 * OSD_OK: success
 */
int coll_delete(struct db_context *dbc, uint64_t pid, uint64_t cid, 
		uint64_t oid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->coll || !dbc->coll->delete)
		return -EINVAL;

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->coll->delete, 1, pid);
	ret |= sqlite3_bind_int64(dbc->coll->delete, 2, cid);
	ret |= sqlite3_bind_int64(dbc->coll->delete, 3, oid);
	ret = exec_dms(dbc, dbc->coll->delete, ret, __func__);
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
int coll_remove_cid(struct db_context *dbc, uint64_t pid, uint64_t cid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->coll || !dbc->coll->remcid)
		return -EINVAL;

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->coll->remcid, 1, pid);
	ret |= sqlite3_bind_int64(dbc->coll->remcid, 2, cid);
	ret = exec_dms(dbc, dbc->coll->remcid, ret, __func__);
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
int coll_remove_oid(struct db_context *dbc, uint64_t pid, uint64_t oid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->coll || !dbc->coll->remoid)
		return -EINVAL;

repeat:
	ret = 0;
	ret |= sqlite3_bind_int64(dbc->coll->remoid, 1, pid);
	ret |= sqlite3_bind_int64(dbc->coll->remoid, 2, oid);
	ret = exec_dms(dbc, dbc->coll->remoid, ret, __func__);
	if (ret == OSD_REPEAT)
		goto repeat;
	return ret;
}

/*
 * tests whether collection is empty. 
 *  <0: in case of error
 * ==1: if collection is empty or absent 
 * ==0: if not empty
 */
int coll_isempty_cid(struct db_context *dbc, uint64_t pid, uint64_t cid)
{
	int ret = 0;
	int count = 0;

	if (!dbc || !dbc->db || !dbc->coll || !dbc->coll->emptycid) {
		ret = -EINVAL;
		goto out;
	}

repeat:
	ret |= sqlite3_bind_int64(dbc->coll->emptycid, 1, pid);
	ret |= sqlite3_bind_int64(dbc->coll->emptycid, 2, cid);
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
	}

	while((ret = sqlite3_step(dbc->coll->emptycid)) == SQLITE_ROW) {
		count = sqlite3_column_int(dbc->coll->emptycid, 0);
	}
	if (ret != SQLITE_DONE) {
		error_sql(dbc->db, "%s: failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
	}

	ret = OSD_OK;

out_reset:
	sqlite3_reset(dbc->coll->emptycid);

out:
	if (ret != OSD_OK)
		return ret;
	else
		return (0 == count);
}


/*
 * returns:
 * -EINVAL: invalid arg, cid is not set
 * OSD_ERROR: in case of any error, cid is not set
 * OSD_OK: success, cid is set to proper collection id
 */
int coll_get_cid(struct db_context *dbc, uint64_t pid, uint64_t oid, 
		 uint32_t number, uint64_t *cid)
{
	int ret = 0;

	if (!dbc || !dbc->db || !dbc->coll || !dbc->coll->getcid) {
		ret = -EINVAL;
		goto out;
	}

	ret |= sqlite3_bind_int64(dbc->coll->getcid, 1, pid);
	ret |= sqlite3_bind_int64(dbc->coll->getcid, 2, oid);
	ret |= sqlite3_bind_int64(dbc->coll->getcid, 3, number);
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
	}

	while((ret = sqlite3_step(dbc->coll->getcid)) == SQLITE_ROW) {
		*cid = sqlite3_column_int64(dbc->coll->getcid, 0);
	}
	if (ret != SQLITE_DONE) {
		error_sql(dbc->db, "%s: failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
	}

	ret = OSD_OK;

out_reset:
	sqlite3_reset(dbc->coll->getcid);
out:
	return ret;
}

/*
 * Collection Attributes Page (CAP) of a userobject stores its membership in 
 * collections osd2r01 Sec 7.1.2.19.
 */
int coll_get_cap(sqlite3 *db, uint64_t pid, uint64_t oid, void *outbuf, 
	       uint64_t outlen, uint8_t listfmt, uint32_t *used_outlen)
{
	return -1;
}


/*
 * returns:
 * -EINVAL: invalid arg
 * OSD_ERROR: other errors
 * OSD_OK: success, oids copied into outbuf, cont_id set if necessary
 */
int coll_get_oids_in_cid(struct db_context *dbc, uint64_t pid, uint64_t cid, 
		       uint64_t initial_oid, uint64_t alloc_len, 
		       uint8_t *outdata, uint64_t *used_outlen,
		       uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	uint64_t len = 0;
	sqlite3_stmt *stmt = NULL;

	if (!dbc || !dbc->db || !dbc->coll || !dbc->coll->getoids) {
		ret = -EINVAL;
		goto out;
	}
	stmt = dbc->coll->getoids;

	ret |= sqlite3_bind_int64(stmt, 1, pid);
	ret |= sqlite3_bind_int64(stmt, 2, cid);
	ret |= sqlite3_bind_int64(stmt, 3, initial_oid);
	if (ret != SQLITE_OK) {
		error_sql(dbc->db, "%s: bind failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
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
	if (ret != SQLITE_DONE) {
		error_sql(dbc->db, "%s: failed", __func__);
		ret = OSD_ERROR;
		goto out_reset;
	}
	*used_outlen = len;
	ret = OSD_OK;

out_reset:
	sqlite3_reset(stmt);
out:
	return ret;
}

