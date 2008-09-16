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

