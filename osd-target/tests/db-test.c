#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "osd-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util.h"
#include "util/util.h"

void test_obj(struct osd_device *osd);
void test_dup_obj(struct osd_device *osd);
void test_attr(struct osd_device *osd);
void test_obj_manip(struct osd_device *osd);
void test_osd_interface(void);

void test_obj(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	if (ret != 0)
		error_errno("%s: obj_insert failed", __func__);

	ret = obj_delete(osd->db, 1, 2);
	if (ret != 0)
		error_errno("%s: obj_delete failed", __func__);
}

void test_dup_obj(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	if (ret != 0)
		error_errno("%s: obj_insert failed", __func__);

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	if (ret != 0)
		error("%s: obj_insert failed as expected", __func__);

	ret = obj_delete(osd->db, 1, 2);
	if (ret != 0)
		error_errno("%s: obj_delete failed", __func__);
}

void test_attr(struct osd_device *osd)
{
	int ret= 0;
	const char *attr = "This is first attr";

	ret = attr_set_attr(osd->db, 1, 1, 2, 12, attr, strlen(attr)+1);
	if (ret != 0)
		error_errno("%s: attr_set_attr failed", __func__);

	void *val = Calloc(1, 1024);
	if (!val)
		error_errno("%s: Calloc failed", __func__);
	ret = attr_get_attr(osd->db, 1, 1, 2, 12, val, 1024);
	if (ret != 0)
		error_errno("%s: attr_get_attr failed", __func__);
	list_entry_t *ent = (list_entry_t *)val;
	printf("retreived: %lu %u %u %u %s\n", 1UL, ent->page, ent->number, 
	       ent->len, (char *)(ent + ATTR_VAL_OFFSET)); 

	/* get non-existing attr */
	ret = attr_get_attr(osd->db, 2, 1, 2, 12, val, 1024);
	if (ret != 0)
		error("%s: attr_get_attr failed as expected", __func__);

	free(val);
}

void test_obj_manip(struct osd_device *osd)
{
	int i = 0;
	int ret = 0;
	int present = 0;
	uint64_t oid = 0;

	for (i =0; i < 4; i++) {
		ret = obj_insert(osd->db, 1, 1<<i, USEROBJECT);
		if (ret != 0)
			error_errno("%s: obj_insert failed", __func__);
	}

	ret = obj_get_nextoid(osd->db, 1, &oid);
	if (ret != 0)
		error_errno("%s: obj_get_nextoid failed", __func__);
	printf("%s: next oid = %llu\n", __func__, llu(oid));	

	/* get nextoid for new (pid, oid) */
	ret = obj_get_nextoid(osd->db, 4, &oid);
	if (ret != 0)
		error_errno("%s: obj_get_nextoid failed", __func__);
	printf("%s: next oid = %llu\n", __func__, llu(oid));	

	for (i =0; i < 4; i++) {
		ret = obj_delete(osd->db, 1, 1<<i);
		if (ret != 0)
			error_errno("%s: obj_delete failed", __func__);
	}
	
	ret = obj_insert(osd->db, 1, 235, USEROBJECT);
	if (ret != 0)
		error_errno("%s: obj_insert failed", __func__);

	present = obj_ispresent(osd->db, 1, 235);
	printf("obj_ispresent = %d\n", present);

	ret = obj_delete(osd->db, 1, 235);
	if (ret != 0)
		error_errno("%s: obj_delete failed", __func__);

	present = obj_ispresent(osd->db, 1, 235);
	printf("obj_ispresent = %d\n", present);
}

void test_osd_interface(void)
{
	int ret = 0;
	const char *root = "/tmp/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	if (ret != 0)
		error_errno("%s: osd_open", __func__);

	test_obj(&osd);
	test_attr(&osd);

	ret = osd_close(&osd);
	if (ret != 0)
		error_errno("%s: osd_close", __func__);
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
	/*test_attr(&osd);*/
	test_dup_obj(&osd);
	/*test_obj_manip(&osd);*/

	ret = db_close(&osd);
	if (ret != 0)
		return ret;

	/*test_osd_interface();*/

	return 0;
}
