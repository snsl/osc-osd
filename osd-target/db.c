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
#include "util/util.h"
#include "attr.h"

static int db_print_pragma(struct osd_device *osd);

/*
 * < 0: error
 * = 0: success
 * = 1: new db opened
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
		goto out;
	}

	if (is_new_db) {
		ret = 1;
		goto out;
	}

	ret = 0;

out:
	return ret;
}

int db_close(struct osd_device *osd)
{
	int ret = 0;

	if (osd->db == NULL)
		return -EINVAL;

	ret = sqlite3_close(osd->db);
	if (ret != SQLITE_OK) {
		osd_error("Failed to close db %s", sqlite3_errmsg(osd->db));
		return ret;
	}

	return SQLITE_OK;
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

static int db_print_pragma(struct osd_device *osd)
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

