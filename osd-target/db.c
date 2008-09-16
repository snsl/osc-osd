#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <errno.h>
#include <sys/stat.h>

#include "util/osd-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "obj.h"
#include "coll.h"
#include "util/util.h"
#include "attr.h"

extern const char osd_schema[];

static int check_membership(void *arg, int count, char **val, 
			    char **colname)
{
	size_t i = 0;
	struct array *arr = arg;
	const char **tables = arr->a;

	for (i = 0; i < arr->ne; i++)
		if (strcmp(tables[i], val[0]) == 0)
			return 0;
	return -1;
}

/*
 * returns:
 * OSD_ERROR: in case no table is found
 * OSD_OK: when all tables exist
 */
static int db_check_tables(struct osd_device *osd)
{
	int i = 0;
	int ret = 0;
	char SQL[MAXSQLEN];
	char *err = NULL;
	const char *tables[] = {"attr", "obj", "coll"};
	struct array arr = {ARRAY_SIZE(tables), tables};

	sprintf(SQL, "SELECT name FROM sqlite_master WHERE type='table' "
		" ORDER BY name;");
	ret = sqlite3_exec(osd->db, SQL, check_membership, &arr, &err);
	if (ret != SQLITE_OK) {
		osd_error("%s: query %s failed: %s", __func__, SQL, err);
		sqlite3_free(err);
		return OSD_ERROR;
	}

	return OSD_OK;
}


/*
 *  <0: error
 * ==0: success
 * ==1: new db opened, caller must initialize tables
 */
int db_open(const char *path, struct osd_device *osd)
{
	int ret;
	struct stat sb;
	char SQL[MAXSQLEN];
	char *err = NULL;
	int is_new_db = 0;

	ret = stat(path, &sb);
	if (ret == 0) {
		if (!S_ISREG(sb.st_mode)) {
			osd_error("%s: path %s not a regular file", 
				  __func__, path);
			ret = 1;
			goto out;
		}
	} else {
		if (errno != ENOENT) {
			osd_error("%s: stat path %s", __func__, path);
			goto out;
		}

		/* sqlite3 will create it for us */
		is_new_db = 1;
	}

	ret = sqlite3_open(path, &(osd->db));
	if (ret != SQLITE_OK) {
		osd_error("%s: open db %s", __func__, path);
		ret = OSD_ERROR;
		goto out;
	}

	osd->dbc = Calloc(1, sizeof(*osd->dbc));
	if (!osd->dbc) {
		ret = -ENOMEM;
		goto out;
	}

	osd->dbc->db = osd->db; /* TODO: unnecessary if dbc is default */

	if (is_new_db) {
		/* build tables from schema file */
		ret = sqlite3_exec(osd->dbc->db, osd_schema, NULL, NULL, &err);
		if (ret != SQLITE_OK) {
			sqlite3_free(err);
			ret = OSD_ERROR;
			goto out;
		}
	} else {
		/* existing db, check for tables */
		ret = db_check_tables(osd);
		if (ret != OSD_OK)
			goto out;
	}

	/* initialize dbc fields */
	ret = 0;
	ret |= coll_initialize(osd->dbc);
	ret |= obj_initialize(osd->dbc);
	if (ret != OSD_OK) {
		ret = OSD_ERROR;
		goto out;
	}

	if (is_new_db) 
		ret = 1;

out:
	return ret;
}


int db_close(struct db_context *dbc)
{
	int ret = 0;

	if (dbc == NULL)
		return -EINVAL;

	ret = 0;
	ret |= coll_finalize(dbc);
	ret |= obj_finalize(dbc);
	if (ret != OSD_OK)
		return OSD_ERROR;

	ret = sqlite3_close(dbc->db);
	if (ret != SQLITE_OK) {
		osd_error("Failed to close db %s", sqlite3_errmsg(dbc->db));
		return ret;
	}

	return SQLITE_OK;
}


int db_begin_txn(struct osd_device *osd)
{
	int ret = 0;
	char *err = NULL;

	ret = sqlite3_exec(osd->db, "BEGIN TRANSACTION;", NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("pragma failed: %s", err);
		sqlite3_free(err);
	}

	return ret;
}


int db_end_txn(struct osd_device *osd)
{
	int ret = 0;
	char *err = NULL;

	ret = sqlite3_exec(osd->db, "END TRANSACTION;", NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		osd_error("pragma failed: %s", err);
		sqlite3_free(err);
	}

	return ret;
}


int db_exec_pragma(struct osd_device *osd)
{
	int ret = 0;
	char *err = NULL;
	char SQL[MAXSQLEN];

	if (osd->db == NULL)
		return -EINVAL;

	sprintf(SQL, "PRAGMA synchronous = OFF;"); /* sync off */
	ret = sqlite3_exec(osd->db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		goto spit_err;

	sprintf(SQL, "PRAGMA auto_vacuum = 1;"); /* reduce db size on delete */
	ret = sqlite3_exec(osd->db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		goto spit_err;

	sprintf(SQL, "PRAGMA count_changes = 0;"); /* ignore count changes */
	ret = sqlite3_exec(osd->db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		goto spit_err;

	sprintf(SQL, "PRAGMA temp_store = 0;"); /* memory as scratchpad */
	ret = sqlite3_exec(osd->db, SQL, NULL, NULL, &err);
	if (ret != SQLITE_OK)
		goto spit_err;

	goto out;

spit_err:
	osd_error("pragma failed: %s", err);
	sqlite3_free(err);

out:
	return ret;
}


static int callback(void *ignore, int count, char **val, char **colname)
{
	printf("%s: PRAGMA %s = %s\n", __func__, colname[0], val[0]);
	return 0;
}


int db_print_pragma(struct osd_device *osd)
{
	int ret = 0;
	char *err = NULL;
	char SQL[MAXSQLEN];

	if (osd->db == NULL)
		return -EINVAL;

	sprintf(SQL, "PRAGMA synchronous;"); 
	ret = sqlite3_exec(osd->db, SQL, callback, NULL, &err);
	sprintf(SQL, "PRAGMA auto_vacuum;"); 
	ret = sqlite3_exec(osd->db, SQL, callback, NULL, &err);
	sprintf(SQL, "PRAGMA count_changes;"); 
	ret = sqlite3_exec(osd->db, SQL, callback, NULL, &err);
	sprintf(SQL, "PRAGMA temp_store;"); 
	ret = sqlite3_exec(osd->db, SQL, callback, NULL, &err);
	return ret;
}


/*
 * print sqlite errmsg 
 */
void __attribute__((format(printf,2,3)))
error_sql(sqlite3 *db, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "osd: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s.\n", sqlite3_errmsg(db));
}

