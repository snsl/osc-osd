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
#include "util.h"

int attr_set_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, const void *val, uint16_t len)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -ENXIO;

	/* FIXME: Why zero? */
	if (number == 0) {
		const uint8_t *s = val;
		int i;

		/* information page, make sure null terminated */
		if (len > 40)
			goto out;

		for (i=0; i<len; i++) {
			if (s[i] == 0) 	break;
		}

		if (i == len) {
			ret = -EFAULT;
			goto out;
		}
	}

	sprintf(SQL, "INSERT OR REPLACE INTO attr VALUES (?, ?, ?, ?, ?);");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, ATTR_PID_COLUMN, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind PID", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, ATTR_OID_COLUMN, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind OID", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, ATTR_PAGE_COLUMN, page);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind PAGE", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int(stmt, ATTR_NUM_COLUMN, number);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind NUM", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_blob(stmt, ATTR_VALUE_COLUMN, val, len, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind VALUE", __func__);
		goto out_finalize;
	}
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: attr exists", __func__);
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

int attr_delete_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		     uint32_t number)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -ENXIO;

	sprintf(SQL, "DELETE FROM attr WHERE "
		"pid = %lu AND oid = %lu AND page = %u AND number = %u;", 
		pid, oid, page, number);
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		error("%s: sqlite3_exec : %s", __func__, err);
		free(err);
	}

	return ret;
}

int attr_delete_all(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -ENXIO;

	sprintf(SQL, "DELETE FROM attr WHERE pid = %lu AND oid = %lu",
		pid, oid);
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		error("%s: sqlite3_exec : %s", __func__, err);
		free(err);
	}

	return ret;
}

static int attr_gather_attr(sqlite3_stmt *stmt, void *outbuf, uint16_t len)
{
	list_entry_t *ent = (list_entry_t *)outbuf;
	assert(len > ATTR_VAL_OFFSET);

	/* FIXME: Why 0,1,2? */
	ent->page = sqlite3_column_int(stmt, 0);
	ent->number = sqlite3_column_int(stmt, 1);
	ent->len = sqlite3_column_bytes(stmt, 2);
	if (ent->len + ATTR_VAL_OFFSET < len)
		memcpy(ent + ATTR_VAL_OFFSET, sqlite3_column_blob(stmt, 2), 
		       ent->len);
	else
		memcpy(ent + ATTR_VAL_OFFSET, sqlite3_column_blob(stmt, 2), 
		       len - ATTR_VAL_OFFSET);

	return SQLITE_OK;
}

/* 40 bytes including terminating NUL */
static const char unidentified_page_identification[40]
= "        unidentified attributes page   ";

int attr_get_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, void *outbuf, uint16_t len)
{
	int ret = -EINVAL;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -ENXIO;

	if (outbuf == NULL)
		return -EFAULT;

	sprintf(SQL, "SELECT page, number, value FROM attr WHERE"
		" pid = ? AND oid = ? AND page = ? AND number = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, ATTR_PID_COLUMN, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind 1", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, ATTR_OID_COLUMN, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind 2", __func__);
		goto out;
	}
	ret = sqlite3_bind_int(stmt, ATTR_PAGE_COLUMN, page);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind 3", __func__);
		goto out;
	}
	ret = sqlite3_bind_int(stmt, ATTR_NUMBER_COLUMN, number);
	if (ret != SQLITE_OK){
		error_sql(db, "%s: bind 4", __func__);
		goto out;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ( (ret = attr_gather_attr(stmt, outbuf, len)) != SQLITE_OK) {
			error_sql(db, "%s: attr_gather_attr", __func__);
			error_sql(db, "%s: stmt = %s", __func__, stmt);
			goto out;
		}
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		goto out;
	} else if (ret == SQLITE_NOTFOUND) {
		error_sql(db, "%s: attr not found", __func__);
		goto out;
	}
	ret = 0; /* success */

	/* XXX:
	 * if number == 0 and page not found, return this:
	 memcpy(outbuf, unidentified_page_identification, 40);
	 */

out:
	/* XXX: Should we finalize if we fail? Also, we lose ret */
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;
	return ret;
}

int attr_get_attr_page(sqlite3 *db, uint64_t pid, uint64_t  oid,
		       uint32_t page, void *outbuf, uint16_t len)
{
	int ret = -EINVAL;
	int it_len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -ENXIO;
	if (outbuf == NULL)
		return -EFAULT;

	sprintf(SQL, "SELECT * FROM attr"
		"WHERE pid = ? AND oid = ? AND page = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, ATTR_PID_COLUMN, pid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind PID", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, ATTR_OID_COLUMN, oid);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind OID", __func__);
		goto out;
	}
	ret = sqlite3_bind_int(stmt, ATTR_PAGE_COLUMN, page);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: bind PAGE", __func__);
		goto out;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((ret = attr_gather_attr(stmt, outbuf, len)) != 0) {
			error_sql(db, "%s: attr_gather_attr", __func__);
			error_sql(db, "%s: stmt = %s", __func__, stmt);
			goto out;
		}
		/* retrieve attr in list entry format; tab 129 Sec 7.1.3.3 */
		it_len = ((list_entry_t *)outbuf)->len + ATTR_VAL_OFFSET;
		len -= it_len;
		outbuf = (char *) outbuf + it_len;
	}
	switch(ret)
	{
	case SQLITE_DONE:
		break;
	case SQLITE_NOTFOUND:
		error_sql(db, "%s: attr not found", __func__);
		ret = -ENOENT;
		goto out;
	default:
		error_sql(db, "%s: sqlite3_step", __func__);
		goto out;
	}

	ret = 0; /* success */

out:
	/* XXX: Should we finalize if we fail? Also, we lose ret */
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;
	return ret;
}
