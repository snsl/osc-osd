#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "obfs-types.h"
#include "attr-mgmt-sqlite.h"


int main()
{
	char path[]="/tmp/osdattr";
	int ret = 0;
	osd_t osd;

	ret = attrdb_open(path, &osd);
	if (ret != 0)
		return ret;
	void *dbh = osd.db;

	if (ret != 0)
		return ret;

	char *attr = "This is first attr";
	ret = attrdb_set_attr(dbh, 1, 1, 2, strlen(attr), attr);
	if (ret != 0)
		return ret;

	void *val = calloc(1, 1024);
	if (!val)
		return -ENOMEM;
	ret = attrdb_get_attr(dbh, 1, 1, 2, 1024, val);
	if (ret != 0)
		return ret;
	list_entry_t *ent = (list_entry_t *)val;
	printf("retreived: %lu %u %u %u %s\n", 1UL, ent->pgnum, ent->num, 
	       ent->len, (char *)(ent + ATTR_T_DSZ)); 
	free(val);

	ret = attrdb_drop_object_attr_tab(dbh, 1);
	if (ret != 0)
		return ret;

	ret = attrdb_close(&osd);
	if (ret != 0)
		return ret;

	return 0;
}
