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
#include "util/util.h"

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
		error("%s: sqlite3_exec : %s", __func__, err);
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
		error("%s: sqlite3_exec : %s", __func__, err);
		sqlite3_free(err);
	}

	return ret;
}

/* 
 * Gather the results into list_entry format. Each row has page, number, len,
 * value. Look at queries in attr_get_attr attr_get_attr_page.
 */
static int attr_gather_attr(sqlite3_stmt *stmt, void *outbuf, uint16_t len)
{
	uint16_t valen = 0;
	list_entry_t *ent = (list_entry_t *)outbuf;
	assert(len > ATTR_VAL_OFFSET);

	set_htonl_le((uint8_t *)&(ent->page), sqlite3_column_int(stmt, 0));
	set_htonl_le((uint8_t *)&(ent->number), sqlite3_column_int(stmt, 1));
	valen = sqlite3_column_bytes(stmt, 2);
	set_htons_le((uint8_t *)&(ent->len), valen);
	if (valen + ATTR_VAL_OFFSET < len)
		memcpy((uint8_t *)ent + ATTR_VAL_OFFSET, 
		       sqlite3_column_blob(stmt, 2), valen);
	else
		memcpy((uint8_t *)ent + ATTR_VAL_OFFSET, 
		       sqlite3_column_blob(stmt, 2), len - ATTR_VAL_OFFSET);

	return 0;
}

/* 40 bytes including terminating NUL */
static const char unidentified_page_identification[40]
= "        unidentified attributes page   ";

int attr_get_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, void *outbuf, uint16_t len)
{
	int ret = -EINVAL, found = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL || outbuf == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT page, number, value FROM attr WHERE"
		" pid = ? AND oid = ? AND page = ? AND number = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
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

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((ret = attr_gather_attr(stmt, outbuf, len)) != SQLITE_OK) {
			error_sql(db, "%s: attr_gather_attr", __func__);
			goto out_finalize;
		}
		found = 1;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		goto out_finalize;
	} else if (found == 0) {
		error("%s: attr (%llu %llu %u %u) not found", __func__,
		      llu(pid), llu(oid), page, number);
		goto out_finalize;
	}

	/* XXX:
	 * if number == 0 and page not found, return this:
	 memcpy(outbuf, unidentified_page_identification, 40);
	 */

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) 
		error_sql(db, "%s: finalize", __func__);
	
out:
	return ret;
}

int attr_get_attr_page(sqlite3 *db, uint64_t pid, uint64_t  oid,
		       uint32_t page, void *outbuf, uint16_t len)
{
	int ret = -EINVAL, found = 0;
	int it_len = 0;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL || outbuf == NULL)
		return -EINVAL;

	sprintf(SQL, "SELECT * FROM attr"
		"WHERE pid = ? AND oid = ? AND page = ?");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: prepare", __func__);
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

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((ret = attr_gather_attr(stmt, outbuf, len)) != 0) {
			error_sql(db, "%s: attr_gather_attr", __func__);
			goto out;
		}
		/* retrieve attr in list entry format; tab 129 Sec 7.1.3.3 */
		it_len = ((list_entry_t *)outbuf)->len + ATTR_VAL_OFFSET;
		len -= it_len;
		if (len < ATTR_VAL_OFFSET)
			break; /* length exceeded, not an error, Sec 5.2.2.2 */
		outbuf = (char *) outbuf + it_len;
		found = 1;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		goto out_finalize;
	} else if (found == 0) {
		error("%s: attr (%llu %llu %u ) not found", __func__,
		      llu(pid), llu(oid), page);
		goto out_finalize;
	}

out_finalize:
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK) 
		error_sql(db, "%s: finalize", __func__);
	
out:
	return ret;
}
