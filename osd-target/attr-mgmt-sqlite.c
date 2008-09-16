#include <obfs-types.h>
#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "attr-mgmt-sqlite.h"
#include "util.h"
#include "obj.h"

static int attrdb_errstmt(const char *errmsg, sqlite3_stmt *stmt, 
				 int ret)
{
	fprintf(stderr,"SQL ERROR: %s\n", errmsg);
	sqlite3_finalize(stmt);
	return ret;
}

static int attrdb_err(char *err, int ret)
{
	fprintf(stderr, "SQL ERROR: %s\n", err);
	sqlite3_free(err);
	return ret;
}

extern const char osd_schema[];

static int initial_populate(osd_t *osd);

int attrdb_open(const char *path, osd_t *osd)
{
	int ret;
	struct stat sb;
	char SQL[MAXSQLEN];
	char *err = NULL;
	int is_new_db = 0;
	sqlite3 *dbp;

	ret = stat(path, &sb);
	if (ret == 0) {
		if (!S_ISREG(sb.st_mode)) {
			error("%s: path %s not a regular file", __func__, path);
			ret = 1;
			goto out;
		}
	} else {
		if (errno != ENOENT) {
			error_errno("%s: stat path %s", __func__, path);
			goto out;
		}

		/* sqlite3 will create it for us */
		is_new_db = 1;
	}

	ret = sqlite3_open(path, &dbp);
	if (ret) {
		error("%s: open db %s", __func__, path);
		goto out;
	}
	osd->db = dbp;

	if (is_new_db) {
		/*
		 * Build tables from schema file.
		 */
		ret = sqlite3_exec(osd->db, osd_schema, NULL, NULL, &err);
		if (ret != SQLITE_OK) 
			return attrdb_err(err, -1);

		ret = initial_populate(osd);
		if (ret)
			goto out;
	}

	ret = 0;

out:
	return ret;
}

/*
 * Things that go as attributes on the root page.
 * XXX: on second thought, don't stick these in the db, just return
 * them as needed programatically.  There's plenty of other variable
 * ones that can't live in the db (clock, #objects, capacity used).
 */
struct init_attr {
	uint32_t page;
	uint32_t number;
	const char *s;
};
static struct init_attr root_info[] = {
	{ ROOT_PG + 0, 0, "INCITS  T10 Root Directory" },
	{ ROOT_PG + 0, ROOT_PG + 1, "INCITS  T10 Root Information" },
	{ ROOT_PG + 1, 0, "INCITS  T10 Root Information" },
	{ ROOT_PG + 1, 3, "\xf1\x81\x00\x0eOSC     OSDEMU" },
	{ ROOT_PG + 1, 4, "OSC" },
	{ ROOT_PG + 1, 5, "OSDEMU" },
	{ ROOT_PG + 1, 6, "9001" },
	{ ROOT_PG + 1, 7, "0" },
	{ ROOT_PG + 1, 8, "1" },
	{ ROOT_PG + 1, 9, "hostname" },
	{ ROOT_PG + 0, ROOT_PG + 2, "INCITS  T10 Root Quotas" },
	{ ROOT_PG + 2, 0, "INCITS  T10 Root Quotas" },
	{ ROOT_PG + 0, ROOT_PG + 3, "INCITS  T10 Root Timestamps" },
	{ ROOT_PG + 3, 0, "INCITS  T10 Root Timestamps" },
	{ ROOT_PG + 0, ROOT_PG + 5, "INCITS  T10 Root Timestamps" },
	{ ROOT_PG + 5, 0, "INCITS  T10 Root Policy/Security" },
};

/*
 * Create root object and attributes for root and partition zero.
 */
static int initial_populate(osd_t *osd)
{
	int ret;
	int i;

	ret = obj_insert(osd, 0, 0);
	if (ret)
	    goto out;

	for (i=0; i<ARRAY_SIZE(root_info); i++) {
		struct init_attr *ia = &root_info[i];
		ret = attrdb_set_attr(osd->db, 0, 0, ia->page, ia->number,
				      ia->s, strlen(ia->s)+1);
		if (ret)
			goto out;
	}

	ret = create_partition(osd, 0);

out:
	return ret;
}

static struct init_attr partition_info[] = {
	{ PARTITION_PG + 0, 0, "INCITS  T10 Partition Directory" },
	{ PARTITION_PG + 0, PARTITION_PG + 1,
	                      "INCITS  T10 Partition Information" },
	{ PARTITION_PG + 1, 0, "INCITS  T10 Partition Information" },
};

int create_partition(osd_t *osd, uint64_t pid)
{
	int i, ret;

	if (pid != 0) {
		/*
		 * Partition zero does not have an entry in the obj db; those
		 * are only for user-created partitions.
		 */
		ret = obj_insert(osd, pid, 0);
		if (ret)
		    goto out;
	}
	for (i=0; i<ARRAY_SIZE(partition_info); i++) {
		struct init_attr *ia = &partition_info[i];
		ret = attrdb_set_attr(osd->db, pid, 0, ia->page, ia->number,
				      ia->s, strlen(ia->s)+1);
		if (ret)
			goto out;
	}

	/* the pid goes here */
	ret = attrdb_set_attr(osd->db, pid, 0, PARTITION_PG + 1, 1, &pid, 8);
	if (ret)
		goto out;

out:
	return ret;
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

int attrdb_creat_object_attr_tab(void *dbh, uint64_t pid, uint64_t oid)
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
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int64(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, oid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -EEXIST);
	ret = sqlite3_finalize(stmt);
	if (ret != SQLITE_OK)
		return -1;
	return 0;
}

int attrdb_drop_object_attr_tab(void *dbh, uint64_t pid, uint64_t oid)
{
	int ret = 0;
	sqlite3 *dbp = (sqlite3 *)dbh;
	char SQL[MAXSQLEN];
	char *err = NULL;

	if (dbp == NULL)
		return -1;

	sprintf(SQL, "DELETE FROM obj WHERE pid = %lu AND oid = %lu",
	        pid, oid);
	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		return attrdb_err(err, -1);

	sprintf(SQL, "DELETE FROM attr WHERE pid = %lu AND oid = %lu",
	        pid, oid);
	ret = sqlite3_exec(dbp, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		return attrdb_err(err, -1);

	return 0;
}

int attrdb_set_attr(void *dbh, uint64_t pid, uint64_t oid, uint32_t page, 
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
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, oid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, page);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 3, number);
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

out:
	return ret;
}

static int attrdb_gather_attr(sqlite3_stmt *stmt, void *outbuf, uint16_t len)
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

/* 40 bytes including terminating NUL */
static const char unidentified_page_identification[40]
    = "        unidentified attributes page   ";

int attrdb_get_attr(void *dbh, uint64_t pid, uint64_t oid, uint32_t page, 
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
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, oid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 1, page);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, number);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (attrdb_gather_attr(stmt, outbuf, len) != 0) {
			return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
		}
	}
	if (ret != SQLITE_DONE)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	/* XXX:
	 * if number == 0 and page not found, return this:
		memcpy(outbuf, unidentified_page_identification, 40);
	 */

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;

	return 0;
}

int attrdb_get_attr_page(void *dbh, uint64_t pid, uint64_t  oid,
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
	            " WHERE pid = ? AND oid = ? AND page = ?");
	ret = sqlite3_prepare(dbp, SQL, strlen(SQL), &stmt, NULL);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	ret = sqlite3_bind_int(stmt, 1, pid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 2, oid);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
	ret = sqlite3_bind_int(stmt, 3, page);
	if (ret != SQLITE_OK)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((ret = attrdb_gather_attr(stmt, outbuf, len)) != 0) {
			return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);
		}
		/* retrieve attr in list entry format; tab 129 Sec 7.1.3.3 */
		it_len = ((list_entry_t *)outbuf)->len + ATTR_T_DSZ;
		len -= it_len;
		outbuf = (char *) outbuf + it_len;
	}
	if (ret != SQLITE_DONE)
		return attrdb_errstmt(sqlite3_errmsg(dbp), stmt, -1);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		return -1;

	return 0;
}
