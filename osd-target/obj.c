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
		return -EINVAL;

	sprintf(SQL, "INSERT INTO obj VALUES(?, ?);");
	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret) {
		error_sql(db, "%s: bind pid", __func__);
		goto out_finalize;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret) {
		error_sql(db, "%s: bind oid", __func__);
		goto out_finalize;
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		/* FIXME: Do we know for certain that the record exists here? */
		ret = -EEXIST;
		error_sql(db, "%s: object %lu %lu exists!", __func__, pid, oid);
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

int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (db == NULL)
		return -EINVAL;

	sprintf(SQL, "DELETE FROM obj WHERE pid = %lu AND oid = %lu",
		pid, oid);
	ret = sqlite3_exec(db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		sqlite3_free(err);
		return ret;
	}

	return SQLITE_OK;
}
