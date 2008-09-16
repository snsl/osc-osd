#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

#include "obfs.h"
#include "util.h"
#include "obj.h"

struct object *obj_lookup(struct osd *osd, uint64_t pid, uint64_t oid)
{
	char SQL[MAXSQLEN];
	sqlite3 *db = osd->db;
	sqlite3_stmt *stmt;
	struct object *obj = NULL;
	int ret;

	sprintf(SQL, "SELECT * FROM obj"
	             " WHERE pid = ? AND oid = ?");
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

int obj_insert(struct osd *osd, uint64_t pid, uint64_t oid)
{
	char SQL[MAXSQLEN];
	sqlite3 *db = osd->db;
	sqlite3_stmt *stmt;
	struct object *obj = NULL;
	int ret;

	obj = Malloc(sizeof(*obj));
	obj->pid = pid;
	obj->oid = oid;

	sprintf(SQL, "INSERT INTO obj"
	             " (pid,oid) VALUES(?, ?)");
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
		if (ret == SQLITE_DONE) {
			ret = 0;
			break;
		} else {
			error_sql(db, "%s: step", __func__);
			goto out;
		}
	}

out:
	return ret;
}

void obj_free(struct object *obj)
{
	free(obj);
}

