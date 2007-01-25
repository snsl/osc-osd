#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <errno.h>
#include <sys/stat.h>

#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "obj.h"
#include "util.h"
#include "attr.h"

extern const char osd_schema[];

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
static int initial_populate(struct osd_device *osd)
{
	int i = 0, ret = 0;

	if (!osd)
		return -ENXIO;

	ret = obj_insert(osd->db, 0, 0);
	if (ret != SQLITE_OK)
		goto out;

	for (i=0; i<ARRAY_SIZE(root_info); i++) {
		struct init_attr *ia = &root_info[i];

		/* FIXME: We may want to define a ROOT_OID constant
		 * just so the code reads better */
		ret = attr_set_attr(osd->db, 0 /*pid*/, 0/*oid*/, ia->page, ia->number,
				    ia->s, strlen(ia->s)+1);
		if (ret != SQLITE_OK)
			goto out;
	}

	ret = osd_create_partition(osd, 0);

out:
	return ret;
}

int db_open(const char *path, struct osd_device *osd)
{
	int ret;
	struct stat sb;
	char SQL[MAXSQLEN];
	char *err = NULL;
	int is_new_db = 0;
	sqlite3 *dbp;

	ret = stat(path, &sb);
	if (ret != 0) {
		if (errno != ENOENT) {
			error_errno("%s: stat path %s", __func__, path);
			goto out;
		}

		/* sqlite3 will create it for us */
		is_new_db = 1;
	}

	ret = sqlite3_open(path, &dbp);
	if (ret != SQLITE_OK) {
		error("%s: open db %s", __func__, path);
		goto out;
	}
	osd->db = dbp;

	if (is_new_db) {
		/*
		 * Build tables from schema file.
		 */
		ret = sqlite3_exec(osd->db, osd_schema, NULL, NULL, &err);
		if (ret != SQLITE_OK) {
			sqlite3_free(err);
			return -1;
		}

		ret = initial_populate(osd);
		if (ret)
			goto out;
	}

	ret = SQLITE_OK;

out:
	return ret;
}

int db_close(struct osd_device *osd)
{
	int ret = 0;
	sqlite3 *dbp = osd->db;

	if (dbp == NULL)
		return -ENXIO;

	ret = sqlite3_close(dbp);
	if (ret != SQLITE_OK) {
		printf("Failed to close db %s\n", sqlite3_errmsg(dbp));
		return ret;
	}

	return SQLITE_OK;
}

