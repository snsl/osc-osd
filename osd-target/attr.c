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

int attr_insert_object(void *dbh, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	if (dbp == NULL)
		return -1;

	/* add object to table */
	sprintf(SQL, "INSERT INTO obj VALUES (?, ?);");
	ret = sqlite3_prepare(dbp, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -EEXIST);
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		return -1;
	return 0;
}

int attr_delete_object(void *dbh, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	sqlite3 *dbp = (sqlite3 *)dbh;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "DELETE FROM attr WHERE pid = %lu AND oid = %lu",
		pid, oid);
	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		return db_err_free(err, -1);

	return 0;
}

int attr_set_attr(void *dbh, uint64_t pid, uint64_t oid, uint32_t page, 
		    uint32_t number, const void *val, uint16_t len)
{
	int ret = 1;
	char SQL[MAXSQLEN];
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	if (number == 0) {
		const uint8_t *s = val;
		int i;

		/* information page, make sure null terminated */
		if (len > 40)
			goto out;
		for (i=0; i<len; i++)
			if (s[i] == 0)
				break;
		if (i == len)
			goto out;
	}

	sprintf(SQL, "INSERT INTO attr VALUES (?, ?, ?, ?, ?);");
	ret = sqlite3_prepare(dbh, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 3, page);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 4, number);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_blob(stmt, 5, val, len, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		return -1;

out:
	return ret;
}

static int attr_gather_attr(sqlite3_stmt *stmt, void *outbuf, uint16_t len)
{
	list_entry_t *ent = (list_entry_t *)outbuf;
	assert(len > ATTR_T_DSZ);

	ent->page = sqlite3_column_int(stmt, 0);
	ent->number = sqlite3_column_int(stmt, 1);
	ent->len = sqlite3_column_int(stmt, 2);
	if (ent->len + ATTR_T_DSZ < len)
		memcpy(ent + ATTR_T_DSZ, sqlite3_column_blob(stmt, 3), 
		       ent->len);
	else
		memcpy(ent + ATTR_T_DSZ, sqlite3_column_blob(stmt, 3), 
		       len - ATTR_T_DSZ);
	return 0;
}

/* 40 bytes including terminating NUL */
static const char unidentified_page_identification[40]
= "        unidentified attributes page   ";

int attr_get_attr(void *dbh, uint64_t pid, uint64_t oid, uint32_t page, 
		    uint32_t number, void *outbuf, uint16_t len)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	sprintf(SQL, "SELECT * FROM attr WHERE"
		" WHERE pid = ? AND oid = ? AND page = ? AND number = ?");
	ret = sqlite3_prepare(dbp, SQL, strlen(SQL), &stmt, NULL);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 1, oid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, page);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, number);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (attr_gather_attr(stmt, outbuf, len) != 0) {
			return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
		}
	}
	if (ret != SQLITE_DONE)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);

	/* XXX:
	 * if number == 0 and page not found, return this:
	 memcpy(outbuf, unidentified_page_identification, 40);
	 */

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;

	return 0;
}

int attr_getall_page_attr(void *dbh, uint64_t pid, uint64_t  oid,
			    uint32_t page, void *outbuf, uint16_t len)
{
	int ret = 0;
	int it_len = 0;
	char SQL[MAXSQLEN];
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "SELECT * FROM attr"
		"WHERE pid = ? AND oid = ? AND page = ?");
	ret = sqlite3_prepare(dbp, SQL, strlen(SQL), &stmt, NULL);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);

	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 3, page);
	if (ret != SQLITE_OK)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((ret = attr_gather_attr(stmt, outbuf, len)) != 0) {
			return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);
		}
		/* retrieve attr in list entry format; tab 129 Sec 7.1.3.3 */
		it_len = ((list_entry_t *)outbuf)->len + ATTR_T_DSZ;
		len -= it_len;
		outbuf = (char *) outbuf + it_len;
	}
	if (ret != SQLITE_DONE)
		return db_err_finalize(sqlite3_errmsg(dbp), stmt, -1);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;

	return 0;
}
