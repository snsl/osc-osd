#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

#include "attr-mgmt-sqlite.h"

int osd_open(const char *root, osd_t *osd)
{
	int ret = 0;

	/* test if root directory is present with perms */
	ret = open(root, O_RDWR | O_CREAT | O_DIRECTORY, S_IRWXU | S_IRGRP);
	if (ret == -1 && errno != EEXIST)
		return -1;
	else if ((ret == -1 && errno == EEXIST) || (ret != -1))
		close(ret);

	if (strlen(root) > MAXROOTLEN)
		return -1;
	char dbname[MAXNAMELEN];
	sprintf(dbname, "%s/attr.db", root);
	ret = attrdb_open(dbname, osd);
	if (ret == -1)
		return ret;
	osd->root = strdup(root);
	return 0;
}

int osd_close(osd_t *osd)
{
	return osd_close(osd);
	free(osd->root);
}

int osd_format(osd_size_t cap)
{
}
