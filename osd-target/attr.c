#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "osd-types.h"
#include "db.h"
#include "attr.h"
#include "util/util.h"
#include "list-entry.h"

/*
 * XXX: to avoid repeated mallocs, create this hack. a cleaner/clever
 * interface with list-entry will avoid this
 */
struct blob_holder {
	void *buf;
	size_t sz;
};

static struct blob_holder attr_blob = {
	.buf = NULL,
	.sz = 0,
};


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
		" pid = %lu AND oid = %lu AND page = %u AND number = %u;", 
		pid, oid, page, number);
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

	sprintf(SQL, "DELETE FROM attr WHERE pid = %lu AND oid = %lu;",
		pid, oid);
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
 * -EINVAL: error
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
		attr_blob.sz = len + ((0x8 - (len & 0x7)) & 0x7);
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

/* 40 bytes including terminating NUL */
static const char unidentified_page_identification[40]
= "        unidentified attributes page   ";

/*
 * get one attribute in list format.
 *
 * -EINVAL, -EIO: error, used_outlen = 0.
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
		osd_error("%s: attr (%llu %llu %u %u) not found", __func__,
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
		if (ret == -EINVAL)
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
 * get all attributes in a list format
 *
 * returns: 
 * -EINVAL, -EIO: error, used_outlen not modified
 * ==0: success, used_outlen modified
 */
int attr_get_all_attrs(sqlite3 *db, uint64_t pid, uint64_t  oid, void *outbuf,
		       uint64_t outlen, uint8_t listfmt, 
		       uint32_t *used_outlen)
{
	/* TODO: */
	return 0;
}

int attr_get_for_all_pages(sqlite3 *db, uint64_t pid, uint64_t  oid, 
			   uint32_t number, void *outbuf, uint64_t outlen,
			   uint8_t listfmt, uint32_t *used_outlen)
{
	/* TODO: */
	return 0;
}
