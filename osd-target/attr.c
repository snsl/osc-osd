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
#include "attr.h"
#include "util/util.h"
#include "list-entry.h"
#include "target-defs.h"

/*
 * XXX: to avoid repeated mallocs, create this hack. a cleaner/clever
 * interface with list-entry will avoid this
 * C-standard says static items are always initialized to zero.
 */
static struct {
	void *buf;
	size_t sz;
} attr_blob;

/* 40 bytes including terminating NUL */
static const char unidentified_page_identification[ATTR_PG_ID_LEN]
= "        unidentified attributes page   ";


int attr_set_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, const void *val, uint16_t len)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "INSERT OR REPLACE INTO attr VALUES (?, ?, ?, ?, ?);");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		goto out; /* no prepared stmt, no need to finalize */
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 3, page);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind page", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 4, number);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind num", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_blob(stmt, 5, val, len, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind value", __func__);
		goto out_finalize;
	}
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: attr exists", __func__);
		goto out_finalize;
	}

out_finalize:
	/* 
	 * NOTE: sqlite3_finalize grabs the correct error code in case of
	 * failure. else it returns 0. hence, value in 'ret' is not lost
	 */
	ret = sqlite3_finalize(stmt); 
	if (ret != SQLITE_OK)
		error_sql(db, "%s: finalize", __func__);

out:
	return ret;
}

int attr_delete_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		     uint32_t number)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM attr WHERE "
		" pid = %llu AND oid = %llu AND page = %u AND number = %u;", 
		llu(pid), llu(oid), page, number);
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

int attr_delete_all(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM attr WHERE pid = %llu AND oid = %llu;",
		llu(pid), llu(oid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

/* 
 * Gather the results into list_entry format. Each row has page, number, len,
 * value. Look at queries in attr_get_attr attr_get_attr_page.  See page 163.
 *
 * -EINVAL: invalid argument
 * -ENOMEM: out of memory
 * -EOVERFLOW: error, if not enough room to even start the entry
 * >0: success. returns number of bytes copied into outbuf.
 */
static int attr_gather_attr(sqlite3_stmt *stmt, void *buf, uint32_t buflen,
			    uint64_t oid, uint8_t listfmt)
{
	int ret = 0;
	uint8_t *cp = buf;
	uint32_t page = sqlite3_column_int(stmt, 0);
	uint32_t number = sqlite3_column_int(stmt, 1);
	uint16_t len = sqlite3_column_bytes(stmt, 2);

	if (attr_blob.sz < len) {
		free(attr_blob.buf);
		attr_blob.sz = roundup8(len);
		attr_blob.buf = Malloc(attr_blob.sz);
		if (!attr_blob.buf)
			return -ENOMEM;
	}
	memcpy(attr_blob.buf, sqlite3_column_blob(stmt, 2), len);

	if (listfmt == RTRVD_SET_ATTR_LIST)
		return le_pack_attr(buf, buflen, page, number, len, 
				    attr_blob.buf);
	else if (listfmt == RTRVD_CREATE_MULTIOBJ_LIST)
		return le_multiobj_pack_attr(buf, buflen, oid, page, number, 
					     len, attr_blob.buf);
	else
		return -EINVAL;
}

/*
 * get one attribute in list format.
 *
 * -EINVAL, -EIO: error, used_outlen = 0.
 * -ENOMEM: out of memory
 * -ENOENT: error, attribute not found
 * ==0: success, used_outlen modified
 */
int attr_get_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, void *outbuf, uint64_t outlen, 
		  uint8_t listfmt, uint32_t *used_outlen)
{
	int ret = -EINVAL, found = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	*used_outlen = 0;

	if (!db || !outbuf || !used_outlen)
		return -EINVAL;

	sprintf(SQL, "SELECT page, number, value FROM attr WHERE"
		" pid = ? AND oid = ? AND page = ? AND number = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		ret = -EIO;
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 3, page);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind page", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 4, number);
	if (ret != SQLITE_OK){
		error_sql(db, "%s: bind number", __func__);
		goto out_finalize;
	}

	found = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/* duplicate rows violate unique constraint of db */
		assert(found == 0);

		found = 1;
		ret = attr_gather_attr(stmt, outbuf, outlen, oid, listfmt);
		if (ret == -EOVERFLOW)
			*used_outlen = 0; /* the list-entry is not filled */
		else if (ret > 0)
			*used_outlen = ret;
		else
			goto out_finalize;
	}

	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	} else if (found == 0) {
		osd_debug("%s: attr (%llu %llu %u %u) not found", __func__,
		          llu(pid), llu(oid), page, number);
		ret = -ENOENT;
		goto out_finalize;
	}

	ret = 0; /* success */

	/* XXX:
	 * if number == 0 and page not found, return this:
	 memcpy(outbuf, unidentified_page_identification, 40);
	 */

out_finalize:
	/* 'ret' must be preserved. */
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}
	
out:
	return ret;
}

/* 
 * get one page in list format
 *
 * returns: 
 * -EINVAL, -EIO: error, used_outlen not modified
 * -ENOMEM: out of memory
 * -ENOENT: error, page not found
 * ==0: success, used_outlen modified
 */
int attr_get_page_as_list(sqlite3 *db, uint64_t pid, uint64_t  oid,
			  uint32_t page, void *outbuf, uint64_t outlen,
			  uint8_t listfmt, uint32_t *used_outlen)
{
	int ret = -EINVAL, found = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (!db || !outbuf || !used_outlen)
		return -EINVAL;

	sprintf(SQL, "SELECT * FROM attr"
		"WHERE pid = ? AND oid = ? AND page = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		ret = -EIO;
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 3, page);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind page", __func__);
		goto out_finalize;
	}

	found = 0;
	*used_outlen = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		ret = attr_gather_attr(stmt, outbuf, outlen, oid, listfmt);
		if (ret == -EINVAL || ret == -ENOMEM)
			goto out_finalize;
		else if (ret == -EOVERFLOW) 
			goto out_success; /* osd2r00 Sec 5.2.2.2 */

		assert(ret > 0);
		outlen -= ret;
		*used_outlen += ret;
		outbuf = (char *) outbuf + ret;
		found = 1;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	} else if (found == 0) {
		osd_error("%s: attr (%llu %llu %u ) not found", __func__,
		      llu(pid), llu(oid), page);
		ret = -ENOENT;
		goto out_finalize;
	}

out_success:
	ret = 0; /* success */

out_finalize:
	/* preserve 'ret' value */
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}

out:
	return ret;
}

/*
 * for each defined page of an object get attribute with specified number
 *
 * returns: 
 * -EINVAL, -EIO: error, used_outlen not modified
 * -ENOMEM: out of memory
 * ==0: success, used_outlen modified
 */
int attr_get_for_all_pages(sqlite3 *db, uint64_t pid, uint64_t  oid, 
			   uint32_t number, void *outbuf, uint64_t outlen,
			   uint8_t listfmt, uint32_t *used_outlen)
{
	int ret = -EINVAL, found = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (!db || !outbuf || !used_outlen)
		return -EINVAL;

	sprintf(SQL, "SELECT * FROM attr"
		"WHERE pid = ? AND oid = ? AND number = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		ret = -EIO;
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 3, number);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind page", __func__);
		goto out_finalize;
	}

	found = 0;
	*used_outlen = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		ret = attr_gather_attr(stmt, outbuf, outlen, oid, listfmt);
		if (ret == -EINVAL || ret == -ENOMEM)
			goto out_finalize;
		else if (ret == -EOVERFLOW) 
			goto out_success; /* osd2r00 Sec 5.2.2.2 */

		assert(ret > 0);
		outlen -= ret;
		*used_outlen += ret;
		outbuf = (char *) outbuf + ret;
		found = 1;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	} else if (found == 0) {
		osd_error("%s: attr (%llu %llu %u ) not found", __func__,
		      llu(pid), llu(oid), number);
		ret = -ENOENT;
		goto out_finalize;
	}

out_success:
	ret = 0; /* success */

out_finalize:
	/* preserve 'ret' value */
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}

out:
	return ret;
}

/*
 * get all attributes for an object in a list format
 *
 * returns: 
 * -EINVAL, -EIO: error, used_outlen not modified
 * -ENOMEM: out of memory
 * ==0: success, used_outlen modified
 */
int attr_get_all_attrs(sqlite3 *db, uint64_t pid, uint64_t  oid, void *outbuf,
		       uint64_t outlen, uint8_t listfmt, 
		       uint32_t *used_outlen)
{
	int ret = -EINVAL, found = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (!db || !outbuf || !used_outlen)
		return -EINVAL;

	sprintf(SQL, "SELECT * FROM attr WHERE pid = ? AND oid = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		ret = -EIO;
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}

	found = 0;
	*used_outlen = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		ret = attr_gather_attr(stmt, outbuf, outlen, oid, listfmt);
		if (ret == -EINVAL || ret == -ENOMEM)
			goto out_finalize;
		else if (ret == -EOVERFLOW) 
			goto out_success; /* osd2r00 Sec 5.2.2.2 */

		assert(ret > 0);
		outlen -= ret;
		*used_outlen += ret;
		outbuf = (char *) outbuf + ret;
		found = 1;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	} else if (found == 0) {
		osd_error("%s: attr (%llu %llu) not found", __func__,
		      llu(pid), llu(oid));
		ret = -ENOENT;
		goto out_finalize;
	}

out_success:
	ret = 0; /* success */

out_finalize:
	/* preserve 'ret' value */
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}

out:
	return ret;
}

static int dir_page_create_v1(sqlite3 *db, uint64_t pid, uint64_t oid, 
			      char *SQL)
{
	int ret = -EINVAL;
	char *err;

	sprintf(SQL, "CREATE VIEW v1 AS SELECT page, value FROM attr "
		"WHERE pid = %llu and oid = %llu and number = 0; ",
		llu(pid), llu(oid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}
	return ret;
}

static int dir_page_create_v2(sqlite3 *db, uint64_t pid, uint64_t oid,
			      char *SQL)
{
	int ret = -EINVAL;
	char *err;

	sprintf(SQL, "CREATE VIEW v2 AS SELECT page FROM attr "
		"WHERE pid = %llu and oid = %llu GROUP BY page "
		"EXCEPT SELECT page FROM v1; ", llu(pid), llu(oid));
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}
	return ret;
}

static inline int dir_page_drop_v1(sqlite3 *db, char *SQL)
{
	int ret = -EINVAL;
	char *err;

	sprintf(SQL, "DROP VIEW v1;");
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}
	return ret;
}

static inline int dir_page_drop_v2(sqlite3 *db, char *SQL)
{
	int ret = -EINVAL;
	char *err;

	sprintf(SQL, "DROP VIEW v2;");
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}
	return ret;
}

/*
 * packs contents of an attribute of dir page
 * 
 * returns:
 * -ENOMEM: out of memory
 * -EINVAL: invalid argument, or length(attribute value) != 40
 * -EOVERFLOW: buflen too small
 * >0: success, number of bytes copied into buf
 */
static int attr_gather_dir_page(sqlite3_stmt *stmt, void *buf, uint32_t buflen,
				uint64_t oid, uint32_t page, uint8_t listfmt)
{
	int ret = 0;
	uint32_t number = sqlite3_column_int(stmt, 0);
	uint16_t len = sqlite3_column_bytes(stmt, 1);

	if (len != ATTR_PG_ID_LEN) 
		return -EINVAL;

	if (attr_blob.sz < len) {
		free(attr_blob.buf);
		attr_blob.sz = roundup8(len);
		attr_blob.buf = Malloc(attr_blob.sz);
		if (!attr_blob.buf)
			return -ENOMEM;
	}
	memcpy(attr_blob.buf, sqlite3_column_blob(stmt, 1), len);

	if (listfmt == RTRVD_SET_ATTR_LIST)
		return le_pack_attr(buf, buflen, page, number, len,
				    attr_blob.buf);
	else if (listfmt == RTRVD_CREATE_MULTIOBJ_LIST)
		return le_multiobj_pack_attr(buf, buflen, oid, page, number,
					     len, attr_blob.buf);
	else
		return -EINVAL;
}

int attr_get_dir_page(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page,
		      void *outbuf, uint64_t outlen, uint8_t listfmt,
		      uint32_t *used_outlen)
{
	/* TODO: */
	int ret = -EINVAL, found = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;
	sqlite3_stmt *stmt = NULL;

	if (!db || !outbuf || !used_outlen)
		return -EINVAL;

	if (page != USEROBJECT_DIR_PG && page != COLLECTION_DIR_PG &&
	    page != PARTITION_DIR_PG && page != ROOT_DIR_PG)
		return -EINVAL;

	ret = dir_page_create_v1(db, pid, oid, SQL);
	if (ret != 0)
		goto out;

	ret = dir_page_create_v2(db, pid, oid, SQL);
	if (ret != 0)
		goto out_drop_v1;

	sprintf(SQL, "SELECT page, ? FROM v2 UNION SELECT * FROM v1;");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		goto out_drop_v2;
	}
	ret = sqlite3_bind_blob(stmt, 1, unidentified_page_identification, 
				sizeof(unidentified_page_identification), 
				SQLITE_TRANSIENT);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		goto out_finalize;
	}


	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		ret = attr_gather_dir_page(stmt, outbuf, outlen, oid, page, 
					   listfmt);
		if (ret == -EINVAL || ret == -ENOMEM)
			goto out_finalize;
		else if (ret == -EOVERFLOW)
			goto out_success;

		assert(ret > 0);
		outlen -= ret;
		*used_outlen += ret;
		outbuf = (char *)outbuf + ret;
		found = 1;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	} else if (found == 0) {
		osd_error("%s: attr no page set for (%llu %llu)", __func__,
		      llu(pid), llu(oid));
		ret = -ENOENT;
		goto out_finalize;
	}

out_success:
	ret = 0; /* success */

	/* 'ret' must be preserved for the rest of this function */
out_finalize:
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}

out_drop_v2:
	dir_page_drop_v2(db, SQL); /* ignore ret */

out_drop_v1:
	dir_page_drop_v1(db, SQL); /* ignore ret */

out:
	return ret;
}

/*
 * return values:
 * -ENOMEM: out of memory
 * -EINVAL: invalid argument
 * 0: success
 */
int attr_run_query(sqlite3 *db, uint64_t cid, struct query_criteria *qc, 
		   void *outdata, uint32_t alloc_len, uint64_t *used_outlen)
{
	int ret = 0;
	int pos = 0;
	char *cp = NULL;
	const char *op = NULL;
	char select_stmt[MAXSQLEN];
	char *SQL = NULL;
	uint8_t *p = NULL;
	uint32_t i = 0;
	uint32_t sqlen = 0;
	uint32_t factor = 1;
	uint64_t len = 0;
	sqlite3_stmt *stmt = NULL;

	if (!db || !qc || !outdata || !used_outlen) {
		ret = -EINVAL;
		goto out;
	}

	if (qc->query_type == 0)
		op = " UNION ";
	else if (qc->query_type == 1)
		op = " INTERSECT ";
	else {
		ret = -EINVAL;
		goto out;
	}

	SQL = Malloc(MAXSQLEN);
	if (SQL == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	cp = SQL;
	sqlen = 0;

#undef QUERY_TYPE
#define QUERY_TYPE (1)

#if QUERY_TYPE == 1
	/* This query is fastest */

	/* 
	 * Build the query with place holders. SQLite does not support
	 * sub-query grouping using parenthesis. Therefore membership of
	 * collection is tested at last with INTERSECT operation.
	 *
	 * XXX:SD the spec does not mention whether min or max values have to
	 * in tested with '<' or '<='. We assume the test are inclusive.
	 */
	cp = SQL;
	sqlen = 0;
	for (i = 0; i < qc->qc_cnt; i++) {
		sprintf(cp, "SELECT oid FROM attr WHERE page = ? AND "
			" number = ? ");
		if (qc->min_len[i] > 0)
			cp = strcat(cp, "AND ? <= value ");
		if (qc->max_len[i] > 0)
			cp = strcat(cp, "AND value <= ? ");

		/* if this is not the last one qce, append op */
		if ((i+1) < qc->qc_cnt) 
			cp = strcat(cp, op);

		sqlen += strlen(cp);
		if (sqlen >= (MAXSQLEN*factor - 100)) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}
		cp = SQL + sqlen;
	}
	if (qc->qc_cnt > 0) {
		sprintf(cp, " INTERSECT ");
		cp += strlen(cp);
	}
	/*
	 * XXX:SD, we assume that if no min or max exist, then the entire
	 * membership of the collection is returned. min and max constraints
	 * are used to reduce the size of result set. The spec does not
	 * define the result when there are no query criteria.
	 */
	sprintf(cp, "SELECT oid FROM object_collection WHERE cid = ? ;");

	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		ret = -EIO;
		goto out;
	}

	/* bind the place holders with values */
	pos = 1;
	for (i = 0; i < qc->qc_cnt; i++) {
		ret = sqlite3_bind_int(stmt, pos, qc->page[i]);
		if (ret != SQLITE_OK) {
			error_sql(db, "%s: bind page @ %d", __func__, pos);
			goto out_finalize;
		}
		pos++;
		ret = sqlite3_bind_int(stmt, pos, qc->number[i]);
		if (ret != SQLITE_OK) {
			error_sql(db, "%s: bind number @ %d", __func__, pos);
			goto out_finalize;
		}
		pos++;

		if (qc->min_len[i] > 0) {
			ret = sqlite3_bind_blob(stmt, pos, qc->min_val[i], 
						qc->min_len[i], 
						SQLITE_TRANSIENT);
			if (ret != SQLITE_OK) {
				error_sql(db, "%s: bind min_val @ %d", 
					  __func__, pos);
				goto out_finalize;
			}
			pos++;
		}
		if (qc->max_len[i] > 0) {
			ret = sqlite3_bind_blob(stmt, pos, qc->max_val[i], 
						qc->max_len[i], 
						SQLITE_TRANSIENT);
			if (ret != SQLITE_OK) {
				error_sql(db, "%s: bind max_val @ %d", 
					  __func__, pos);
				goto out_finalize;
			}
			pos++;
		}
	}
	ret = sqlite3_bind_int64(stmt, pos, cid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind cid", __func__);
		goto out_finalize;
	}

#elif QUERY_TYPE == 2

	/* build the SQL statment */
	sprintf(select_stmt, "SELECT oc.oid FROM object_collection AS oc, "
		" attr WHERE oc.cid = %llu AND oc.oid = attr.oid ", llu(cid));
	sprintf(cp, select_stmt);
	sqlen += strlen(cp);
	cp += sqlen;
	for (i = 0; i < qc->qc_cnt; i++) {
		sprintf(cp, " AND attr.page = %u AND attr.number = %u ",
			qc->page[i], qc->number[i]);
		if (qc->min_len[i] > 0)
			cp = strcat(cp, " AND ? <= attr.value ");
		if (qc->max_len[i] > 0)
			cp = strcat(cp, " AND attr.value <= ? ");
		if ((i+1) < qc->qc_cnt) {
			cp = strcat(cp, op);
			cp = strcat(cp, select_stmt);
		}
		sqlen += strlen(cp);

		if (sqlen >= (MAXSQLEN*factor - 400)) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}
		cp = SQL + sqlen;
	}
	cp = strcat(cp, " GROUP BY oc.oid ORDER BY oc.oid;");

	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		ret = -EIO;
		goto out;
	}

	/* bind the values */
	pos = 1;
	for (i = 0; i < qc->qc_cnt; i++) {
		if (qc->min_len[i] > 0) {
			ret = sqlite3_bind_blob(stmt, pos, qc->min_val[i], 
						qc->min_len[i], 
						SQLITE_TRANSIENT);
			if (ret != SQLITE_OK) {
				error_sql(db, "%s: bind min_val @ %d", 
					  __func__, pos);
				goto out_finalize;
			}
			pos++;
		}
		if (qc->max_len[i] > 0) {
			ret = sqlite3_bind_blob(stmt, pos, qc->max_val[i], 
						qc->max_len[i], 
						SQLITE_TRANSIENT);
			if (ret != SQLITE_OK) {
				error_sql(db, "%s: bind max_val @ %d", 
					  __func__, pos);
				goto out_finalize;
			}
			pos++;
		}
	}

#endif

	/* execute the query */
	p = outdata;
	p += ML_ODL_OFF; 
	len = ML_ODL_OFF - 8; /* subtract len of addition_len */
	*used_outlen = ML_ODL_OFF;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((alloc_len - len) > 8) {
			/* 
			 * TODO: query is a multi-object command, so delete
			 * the objects from the collection, once they are
			 * selected
			 */
			set_htonll(p, sqlite3_column_int64(stmt, 0));
			*used_outlen += 8;
		}
		p += 8; 
		if (len + 8 < len)
			len = 0xFFFFFFFFFFFFFFFF; /* osd2r01 Sec 6.18.3 */
		else
			len += 8;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	}
	ret = 0;
	set_htonll(outdata, len);

out_finalize:
	ret = sqlite3_finalize(stmt); 
	if (ret != SQLITE_OK)
		error_sql(db, "%s: finalize", __func__);

out:
	free(SQL);
	return ret;
}


int attr_list_oids_attr(sqlite3 *db, uint64_t pid, uint64_t initial_oid, 
			struct getattr_list *get_attr, uint64_t alloc_len,
			void *outdata, uint64_t *used_outlen, 
			uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	char *cp = NULL;
	char *SQL = NULL;
	uint64_t len = 0;
	uint64_t oid = 0;
	uint8_t *head = NULL, *tail = NULL;
	uint32_t i = 0;
	uint32_t factor = 1;
	uint32_t attr_list_len = 0; /*XXX:SD see below */
	uint32_t sqlen = 0;
	sqlite3_stmt *stmt = NULL;

	if (!db || !get_attr || !outdata || !used_outlen || !add_len) {
		ret = -EINVAL;
		goto out;
	}

	if (get_attr->sz == 0) {
		ret = -EINVAL;
		goto out;
	}

	SQL = Malloc(MAXSQLEN);
	if (!SQL) {
		ret = -ENOMEM;
		goto out;
	}

#undef LIST_QUERY_TYPE
#define LIST_QUERY_TYPE (1)
#if LIST_QUERY_TYPE == 1
	/* this is fastest of the two queries */
	cp = SQL;
	sqlen = 0;
	sprintf(SQL, "SELECT obj.oid, attr.page, attr.number, attr.value "
		" FROM obj, attr WHERE attr.pid = %llu AND obj.oid >= %llu "
		" AND obj.type = %u AND obj.pid = attr.pid AND "
		" obj.oid = attr.oid AND ( (page = %u AND number = %u) OR ", 
		llu(pid), llu(initial_oid), USEROBJECT, USER_TMSTMP_PG, 0);
	sqlen += strlen(SQL);
	cp += sqlen;

	for (i = 0; i < get_attr->sz; i++) {
		sprintf(cp, " (attr.page = %u AND attr.number = %u) ",
			get_attr->le[i].page, get_attr->le[i].number);
		if (i < (get_attr->sz - 1))
			strcat(cp, " OR ");
		sqlen += strlen(cp);
		if (sqlen > MAXSQLEN*factor - 100) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}

		cp = SQL + sqlen;
	}
	sprintf(cp, " ) ORDER BY attr.oid; ");

#elif LIST_QUERY_TYPE == 2

	cp = SQL;
	sqlen = 0;
	sprintf(SQL, "SELECT oid, page, number, value FROM attr WHERE "
		" pid = %llu AND oid IN (SELECT oid FROM obj WHERE "
		" pid = %llu AND oid >= %llu AND type = %u) AND "
		" ( (page = %u AND  number = %u) OR ", llu(pid), llu(pid), 
		llu(initial_oid), USEROBJECT, USER_TMSTMP_PG, 0);
	sqlen += strlen(SQL);
	cp += sqlen;

	for (i = 0; i < get_attr->sz; i++) {
		sprintf(cp, " (page = %u AND number = %u) ",
			get_attr->le[i].page, get_attr->le[i].number);
		if (i < (get_attr->sz - 1))
			strcat(cp, " OR ");
		sqlen += strlen(cp);
		if (sqlen > MAXSQLEN*factor - 100) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}

		cp = SQL + sqlen;
	}
	sprintf(cp, " ) ORDER BY attr.oid; ");

#elif LIST_QUERY_TYPE == 3

	cp = SQL;
	sqlen = 0;
	sprintf(SQL, "SELECT obj.oid, attr.page, attr.number, attr.value "
		" FROM obj, attr WHERE ( (page = %u AND number = %u) OR ", 
		USER_TMSTMP_PG, 0);
	sqlen += strlen(SQL);
	cp += sqlen;

	for (i = 0; i < get_attr->sz; i++) {
		sprintf(cp, " (attr.page = %u AND attr.number = %u) ",
			get_attr->le[i].page, get_attr->le[i].number);
		if (i < (get_attr->sz - 1))
			strcat(cp, " OR ");
		sqlen += strlen(cp);
		if (sqlen > MAXSQLEN*factor - 100) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}

		cp = SQL + sqlen;
	}
	sprintf(cp, " ) AND attr.pid = %llu AND obj.oid >= %llu  AND "
		" obj.type = %u AND obj.pid = attr.pid AND "
		" obj.oid = attr.oid ORDER BY attr.oid; ", llu(pid), 
		llu(initial_oid), USEROBJECT);

#elif LIST_QUERY_TYPE == 4

	cp = SQL;
	sqlen = 0;
	sprintf(SQL, "SELECT oid, page, number, value FROM attr "
		" WHERE pid = %llu AND oid >= %llu  AND "
		" ( (page = %u AND number = %u) OR ", llu(pid), 
		llu(initial_oid), USER_TMSTMP_PG, 0);
	sqlen += strlen(SQL);
	cp += sqlen;

	for (i = 0; i < get_attr->sz; i++) {
		sprintf(cp, " (page = %u AND number = %u) ",
			get_attr->le[i].page, get_attr->le[i].number);
		if (i < (get_attr->sz - 1))
			strcat(cp, " OR ");
		sqlen += strlen(cp);
		if (sqlen > MAXSQLEN*factor - 100) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}

		cp = SQL + sqlen;
	}
	sprintf(cp, " ) ORDER BY oid; ");

#endif

	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		goto out;
	}

	/* execute the statement */
	head = tail = outdata;
	attr_list_len = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/*
		 * XXX:SD The spec is inconsistent in applying padding and
		 * alignment rules. Here we make changes to the spec. In our
		 * case object descriptor format header (table 79) is 16B
		 * instead of 12B, and attributes list length field is 4B
		 * instead of 2B as defined in spec, and starts at byte 12
		 * in the header (not 10).  ODE is object descriptor entry.
		 */
		uint64_t oid = sqlite3_column_int64(stmt, 0);
		uint32_t page = sqlite3_column_int64(stmt, 1);
		uint32_t number = sqlite3_column_int64(stmt, 2);
		uint16_t len = sqlite3_column_bytes(stmt, 3);

		if (page == USER_TMSTMP_PG && number == 0) {
			/* look ahead in the buf to see if there is space */
			if (alloc_len >= 16) {
				/* start attr list of 'this' ODE */
				set_htonll(tail, oid);
				memset(tail + 8, 0, 4);  /* reserved */
				if (head != tail) {
					/* fill attr_list_len of prev ODE */
					set_htonl(head, attr_list_len);
					head = tail;
					attr_list_len = 0;
				}
				alloc_len -= 16;
				tail += 16;
				head += 12;  /* points to attr-list-len */
				*used_outlen += 16;
			} else {
				if (head != tail) {
					/* fill attr_list_len of prev ODE */
					set_htonl(head, attr_list_len);
					head = tail;
					attr_list_len = 0;
				}
				if (*cont_id == 0)
					*cont_id = oid;
			}
			*add_len += 16;
			continue;
		} 
		if (alloc_len >= 16) {
			if (attr_blob.sz < len) {
				free(attr_blob.buf);
				attr_blob.sz = roundup8(len);
				attr_blob.buf = Malloc(attr_blob.sz);
				if (!attr_blob.buf)
					return -ENOMEM;
			}
			memcpy(attr_blob.buf, sqlite3_column_blob(stmt, 3), 
			       len);
/* 			osd_debug("%s: oid %llu, page %u, number %u, len %u", 
				  __func__, llu(oid), page, number, len); */
			ret = le_pack_attr(tail, alloc_len, page, number, len, 
					   attr_blob.buf);
			assert (ret != -EOVERFLOW);
			if (ret > 0) {
				alloc_len -= ret;
				tail += ret;
				attr_list_len += ret;
				*used_outlen += ret;
				if (alloc_len < 16){
					set_htonl(head, attr_list_len);
					head = tail;
					attr_list_len = 0;
					if (*cont_id == 0)
						*cont_id = oid;
				}
			} else {
				goto out_finalize;
			}
		} else {
			if (head != tail) {
				/* fill attr_list_len of this ODE */
				set_htonl(head, attr_list_len);
				head = tail;
				attr_list_len = 0;
				if (*cont_id == 0)
					*cont_id = oid;
			}
		}
		*add_len += roundup8(4+4+2+len);
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: query execution failed. SQL %s, add_len "
			  " %llu attr_list_len %u", __func__, SQL, 
			  llu(*add_len), attr_list_len);
		goto out_finalize;
	}
	if (head != tail) {
		set_htonl(head, attr_list_len);
		head += (4 + attr_list_len);
		assert(head == tail);
	}

	ret = 0;

out_finalize:
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}

out:
	free(SQL);
	return ret;
}

int attr_set_member_attrs(sqlite3 *db, uint64_t pid, uint64_t cid, 
			  struct setattr_list *set_attr)
{
	int ret = 0;
	int factor = 1;
	uint32_t i = 0;
	char *cp = NULL;
	char *SQL = NULL;
	size_t sqlen = 0;
	sqlite3_stmt *stmt = NULL;

	if (db == NULL || set_attr == NULL) {
		ret = -EINVAL;
		goto out;
	}

	if (set_attr->sz == 0) {
		ret = 0;
		goto out;
	}

	SQL = Malloc(MAXSQLEN*factor);
	if (!SQL) {
		ret = -ENOMEM;
		goto out;
	}

	cp = SQL;
	sqlen = 0;
	sprintf(SQL, "INSERT OR REPLACE INTO attr ");
	sqlen += strlen(SQL);
	cp += sqlen;
	
	for (i = 0; i < set_attr->sz; i++) {
		sprintf(cp, " SELECT %llu, oid, %u, %u, ? FROM "
			" object_collection WHERE cid = %llu ", llu(pid), 
			set_attr->le[i].page, set_attr->le[i].number, 
			llu(cid));
		if (i < (set_attr->sz - 1))
			strcat(cp, " UNION ALL ");
		sqlen += strlen(cp);
		if (sqlen > (MAXSQLEN*factor - 100)) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}
		cp = SQL + sqlen;
	}
	strcat(cp, " ;");

	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		goto out;
	}

	/* bind values */
	for (i = 0; i < set_attr->sz; i++) {
		ret = sqlite3_bind_blob(stmt, i+1, set_attr->le[i].cval, 
					set_attr->le[i].len,
					SQLITE_TRANSIENT);
		if (ret != SQLITE_OK) {
			error_sql(db, "%s: blob @ %u", __func__, i+1);
			goto out_finalize;
		}
	}

	/* execute the statement */
	while ((ret = sqlite3_step(stmt)) == SQLITE_BUSY);
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	}
	ret = 0;

out_finalize:
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		error_sql(db, "%s: finalize", __func__);

out:
	free(SQL);
	return ret;
}


int attr_get_attr_value(sqlite3 *db, uint64_t pid, uint64_t oid, 
			uint32_t page, uint32_t number, void *outdata, 
			uint16_t len)
{
	int ret = 0;
	int found = 0;
	uint32_t l = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL || outdata == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT value FROM attr WHERE pid = ? AND oid = ?"
		" AND page = ? AND number = ?;");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		ret = -EIO;
		goto out; 
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 3, page);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind page", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, 4, number);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind num", __func__);
		goto out_finalize;
	}

	found = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/* duplicate rows violate unique constraint of db */
		assert(found == 0);
		found = 1;
		l = sqlite3_column_bytes(stmt, 0);
		if (len < l)
			l = len;
		memcpy(outdata, sqlite3_column_blob(stmt, 0), l);
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	} else if (found == 0) {
		osd_debug("%s: attr (%llu %llu %u %u) not found", __func__,
		          llu(pid), llu(oid), page, number);
		ret = -ENOENT;
		goto out_finalize;
	}

	ret = 0; /* success */

out_finalize:
	/* 'ret' must be preserved. */
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}
	
out:
	return ret;
}


