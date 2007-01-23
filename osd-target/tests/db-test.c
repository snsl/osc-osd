#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "osd-types.h"
#include "db.h"
#include "attr.h"
#include "obj.h"

int main()
{
	char path[]="/tmp/osd";
	int ret = 0;
	osd_t osd;

	ret = db_open(path, &osd);
	if (ret != 0)
		return ret;

	ret = obj_insert(&osd, 1, 2);
	if (ret != 0)
		return ret;

/*	const char *attr = "This is first attr";
	ret = attr_set_attr(dbh, 1, 1, 2, strlen(attr), attr);
	if (ret != 0)
		return ret;

	void *val = calloc(1, 1024);
	if (!val)
		return -ENOMEM;
	ret = attr_get_attr(dbh, 1, 1, 2, 1024, val);
	if (ret != 0)
		return ret;
	list_entry_t *ent = (list_entry_t *)val;
	printf("retreived: %lu %u %u %u %s\n", 1UL, ent->pgnum, ent->num, 
	       ent->len, (char *)(ent + ATTR_T_DSZ)); 
	free(val);*/

	ret = obj_delete(&osd, 1, 2);
	if (ret != 0)
		return ret;

	ret = db_close(&osd);
	if (ret != 0)
		return ret;

	return 0;
}
