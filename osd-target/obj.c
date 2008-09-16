#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sqlite3.h>

#include "osd.h"
#include "util.h"
#include "obj.h"
#include "db.h"

obj_id_t *obj_lookup(osd_t *osd, uint64_t pid, uint64_t oid)
{
	int ret;
	char SQL[MAXSQLEN];
	sqlite3 *db = osd->db;
	sqlite3_stmt *stmt;
	obj_id_t *obj = NULL;

	sprintf(SQL, "SELECT * FROM obj WHERE pid = ? AND oid = ?;");
	ret = sqlite3_prepare(db, SQL, -1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret) {
		error_sql(db, "%s: bind 1", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 2, pid);
	if (ret) {
		error_sql(db, "%s: bind 2", __func__);
		goto out;
	}
	for (;;) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_ROW) {
			if (obj) {
				error("%s: multiple matches", __func__);
				continue;
			}
			obj = Malloc(sizeof(*obj));
			obj->pid = pid;
			obj->oid = oid;
		} else if (ret == SQLITE_DONE) {
			break;
		} else {
			error_sql(db, "%s: step", __func__);
			goto out;
		}
	}

out:
	ret = sqlite3_finalize(stmt);
	if (ret)
		error_sql(db, "%s: finalize", __func__);
	return obj;
}

int obj_insert(osd_t *osd, uint64_t pid, uint64_t oid)
{
	int ret;
	char SQL[MAXSQLEN];
	sqlite3 *db = osd->db;
	sqlite3_stmt *stmt;

	sprintf(SQL, "INSERT INTO obj (pid,oid) VALUES(?, ?);");
	ret = sqlite3_prepare(db, SQL, -1, &stmt, NULL);
	if (ret) {
		error_sql(db, "%s: prepare", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret) {
		error_sql(db, "%s: bind 1", __func__);
		goto out;
	}
	ret = sqlite3_bind_int64(stmt, 2, oid);
	if (ret) {
		error_sql(db, "%s: bind 2", __func__);
		goto out;
	}
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		ret = -EEXIST;
		error_sql(db, "%s: %lu %lu exists!", __func__, pid, oid);
		goto out;
	} 
	ret = 0;
out:
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;
	return ret;
}

int obj_delete(osd_t *osd, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;
	sqlite3 *dbp = osd->db;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "DELETE FROM obj WHERE pid = %lu AND oid = %lu",
		pid, oid);
	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		return db_err_free(err, -1);

	return 0;
}

