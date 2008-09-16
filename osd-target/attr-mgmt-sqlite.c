#include <obfs-types.h>
#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "attr-mgmt-sqlite.h"

static inline int attrdb_errstmt(const char *errmsg, sqlite3_stmt *stmt, 
				 int ret)
{
	fprintf(stderr,"SQL ERROR: %s\n", errmsg);
	sqlite3_finalize(stmt);
	return ret;
}

static inline int attrdb_err(char *err, int ret)
{
	fprintf(stderr, "SQL ERROR: %s\n", err);
	sqlite3_free(err);
	return ret;
}

int attrdb_init(const char *path, osd_t *osd)
{
	int ret = 0;
	sqlite3 *dbp = NULL;
	char SQL[MAXSQLEN];
	char *err = NULL;

	ret = sqlite3_open(path, &dbp);
	if (ret != SQLITE_OK) {
		printf("Failed to open db: %s\n", sqlite3_errmsg(dbp));
		return -1;
	}

	sprintf(SQL, "CREATE TABLE IF NOT EXISTS object_tab ("
		"	id INTEGER,"
		"	type INTEGER,"
		"	PRIMARY KEY (id));");
	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) 
		return attrdb_err(err, -1);

	osd->db = dbp;
	return 0;
}

int attrdb_close(osd_t *osd)
{
	int ret = 0;
	sqlite3 *dbp = (sqlite3 *)osd->db;

	if (dbp == NULL)
		return -1;

	ret = sqlite3_close(dbp);
	if (ret != SQLITE_OK) {
		printf("Failed to close db %s\n", sqlite3_errmsg(dbp));
		return -1;
	}

	return 0;
}

int attrdb_creat_object_attr_tab(void *dbh, object_id_t id, object_t type)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	if (dbp == NULL)
		return -1;

	/* add object to object_tab */
	sprintf(SQL, "INSERT INTO object_tab VALUES (?, ?);");
	ret = sqlite3_prepare(dbp, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 1, id);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, type);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -EEXIST);
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		return -1;

	/* create attr table for the object */
	memset(SQL, 0, strlen(SQL));
	sprintf(SQL, "CREATE TABLE IF NOT EXISTS attr_%lu ("
		"	attr_pg INTEGER,"
		"	attr_num INTEGER,"
		"	attr_len INTEGER,"
		"	attr_val BLOB,"
		"	PRIMARY KEY (attr_pg, attr_num));", id);

	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK) 
		return attrdb_err(err, -1);

	return 0;
}

int attrdb_drop_object_attr_tab(void *dbh, object_id_t id)
{
	int ret = 0;
	sqlite3 *dbp = (sqlite3 *)dbh;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "DROP TABLE IF EXISTS attr_%lu;", id);
	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		return attrdb_err(err, -1);

	memset(SQL, 0, strlen(SQL));
	sprintf(SQL, "DELETE FROM object_tab WHERE id = %lu;", id);
	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		return attrdb_err(err, -1);

	return 0;
}

int attrdb_set_attr(void *dbh, object_id_t id, attr_pgnum_t pg, 
		    attr_num_t num, attr_val_len_t len, void *val)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "INSERT INTO attr_%lu VALUES (?, ?, ?, ?);", id);
	ret = sqlite3_prepare(dbh, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, pg);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, num);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 3, len);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_blob(stmt, 4, val, len, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		return -1;

	return 0;
}

static int attrdb_gather_attr(sqlite3_stmt *stmt, attr_val_len_t len, 
			      void *outbuf)
{
	list_entry_t *ent = (list_entry_t *)outbuf;
	assert(len > ATTR_T_DSZ);

	ent->pgnum = sqlite3_column_int(stmt, 0);
	ent->num = sqlite3_column_int(stmt, 1);
	ent->len = sqlite3_column_int(stmt, 2);
	if (ent->len + ATTR_T_DSZ < len)
		memcpy(ent + ATTR_T_DSZ, sqlite3_column_blob(stmt, 3), 
		       ent->len);
	else
		memcpy(ent + ATTR_T_DSZ, sqlite3_column_blob(stmt, 3), 
		       len - ATTR_T_DSZ);
	return 0;
}

int attrdb_get_attr(void *dbh, object_id_t id, attr_pgnum_t pg, 
		    attr_num_t num, attr_val_len_t len, void *outbuf)
{
	int ret = 0;
	char SQL[MAXSQLEN];
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "SELECT * FROM attr_%lu WHERE"
		"	attr_pg = ? and attr_num = ?;", id);
	ret = sqlite3_prepare(dbp, SQL, strlen(SQL), &stmt, NULL);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, pg);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, num);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	while((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (attrdb_gather_attr(stmt, len, outbuf) != 0) {
			return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
		}
	}
	if (ret != SQLITE_DONE)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;

	return 0;
}

int attrdb_get_attr_page(void *dbh, object_id_t id, attr_pgnum_t pg, 
			 attr_val_len_t len, void *outbuf)
{
	int ret = 0;
	int it_len = 0;
	char SQL[MAXSQLEN];
	sqlite3 *dbp = (sqlite3 *)dbh;
	sqlite3_stmt *stmt = NULL;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "SELECT * FROM attr_%lu WHERE attr_pg = ?;", id);
	ret = sqlite3_prepare(dbp, SQL, strlen(SQL), &stmt, NULL);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	ret = sqlite3_bind_int(stmt, 1, pg);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((ret = attrdb_gather_attr(stmt, len, outbuf)) != 0) {
			return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
		}
		it_len = ((list_entry_t *)outbuf)->len + ATTR_T_DSZ;
		len -= it_len, outbuf += it_len;
	}
	if (ret != SQLITE_DONE)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;

	return 0;
}
