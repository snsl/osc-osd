#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include "osd-types.h"
#include "osd.h"
#include "util.h"
#include "obj.h"
#include "db.h"

int obj_insert(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret;
	char SQL[MAXSQLEN];
	sqlite3_stmt *stmt = NULL;

	if (db == NULL)
		return -1;

	sprintf(SQL, "INSERT INTO obj VALUES(?, ?);");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, OBJ_PID_COLUMN, pid);
	if (ret) {
		error_sql(db, "%s: bind pid", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, OBJ_OID_COLUMN, oid);
	if (ret) {
		error_sql(db, "%s: bind oid", __func__);
		goto out;
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		/* FIXME: Do we know for certain that the record exists here? */
		ret = -EEXIST;
		error_sql(db, "%s: %lu %lu exists!", __func__, pid, oid);
		goto out;
	} 
	ret = 0;

out:
	/* FIXME: Should we always finalize? We lose ret here too */
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;
	return ret;
}

int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -ENXIO;

	sprintf(SQL, "DELETE FROM obj WHERE pid = %lu AND oid = %lu",
		pid, oid);
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		sqlite3_free(err);
		return ret;
	}

	return SQLITE_OK;
}
