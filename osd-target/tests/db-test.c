#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "osd-types.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util.h"

void test_obj(struct osd_device *osd);
void test_attr(struct osd_device *osd);

void test_obj(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 2);
	if (ret != 0)
		error_fatal("%s: obj_insert failed", __func__);

	ret = obj_delete(osd->db, 1, 2);
	if (ret != 0)
		error_fatal("%s: obj_delete failed", __func__);

}

void test_attr(struct osd_device *osd)
{
	int ret= 0;
	const char *attr = "This is first attr";

	ret = attr_set_attr(osd->db, 1, 1, 2, 12, attr, strlen(attr)+1);
	if (ret != 0)
		error_fatal("%s: attr_set_attr failed", __func__);

	void *val = Calloc(1, 1024);
	if (!val)
		error_fatal("%s: Calloc failed", __func__);
	ret = attr_get_attr(osd->db, 1, 1, 2, 12, val, 1024);
	if (ret != 0)
		error_fatal("%s: attr_get_attr failed", __func__);
	list_entry_t *ent = (list_entry_t *)val;
	printf("retreived: %lu %u %u %u %s\n", 1UL, ent->page, ent->number, 
	       ent->len, (char *)(ent + ATTR_VAL_OFFSET)); 
	free(val);
}

int main()
{
	char path[]="/tmp/osd";
	int ret = 0;
	struct osd_device osd;

	ret = db_open(path, &osd);
	if (ret != 0)
		return ret;

	/*test_obj(&osd);*/
	test_attr(&osd);

	ret = db_close(&osd);
	if (ret != 0)
		return ret;

	return 0;
}
