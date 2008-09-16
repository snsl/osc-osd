#define _GNU_SOURCE  /* O_DIRECTORY */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "obfs.h"
#include "attr-mgmt-sqlite.h"

int osd_open(const char *root, osd_t *osd)
{
	int ret;
	char dbname[MAXNAMELEN];
	struct stat sb;

	if (strlen(root) > MAXROOTLEN) {
		ret = -ENAMETOOLONG;
		goto out;
	}

	/* test if it exists and is a directory */
	ret = stat(root, &sb);
	if (ret == 0) {
		if (!S_ISDIR(sb.st_mode)) {
			fprintf(stderr, "%s: root %s not a directory\n",
			        __func__, root);
			ret = -ENOTDIR;
			goto out;
		}
	} else {

		if (errno != ENOENT) {
			fprintf(stderr, "%s: stat root %s: %m\n",
			        __func__, root);
			ret = -ENOTDIR;
			goto out;
		}

		/* if not, create it */
		ret = mkdir(root, 0777);
		if (ret < 0) {
			fprintf(stderr, "%s: create root %s: %m\n",
			        __func__, root);
			goto out;
		}
	}

	/* auto-creates db if necessary */
	/* initializes osd->db */
	sprintf(dbname, "%s/attr.db", root);
	ret = attrdb_open(dbname, osd);
	if (ret < 0)
		goto out;

	osd->root = strdup(root);
out:
	return ret;
}

int osd_close(osd_t *osd)
{
	int ret;
	
	ret = attrdb_close(osd);
	free(osd->root);
	return ret;
}

int osd_format(uint64_t cap)
{
	return 0;
}

