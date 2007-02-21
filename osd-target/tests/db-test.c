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
#include "util/util.h"

void test_obj(struct osd_device *osd);
void test_dup_obj(struct osd_device *osd);
void test_attr(struct osd_device *osd);
void test_obj_manip(struct osd_device *osd);
void test_pid_isempty(struct osd_device *osd);
void test_get_obj_type(struct osd_device *osd);
void test_osd_interface(void);

void test_obj(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	if (ret != 0)
		osd_error_errno("%s: obj_insert failed", __func__);

	ret = obj_delete(osd->db, 1, 2);
	if (ret != 0)
		osd_error_errno("%s: obj_delete failed", __func__);
}

void test_dup_obj(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	if (ret != 0)
		osd_error_errno("%s: obj_insert failed", __func__);

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	if (ret != 0)
		osd_error("%s: obj_insert failed as expected", __func__);

	ret = obj_delete(osd->db, 1, 2);
	if (ret != 0)
		osd_error_errno("%s: obj_delete failed", __func__);
}

void test_attr(struct osd_device *osd)
{
	int ret= 0;
	const char *attr = "This is first attr";

	ret = attr_set_attr(osd->db, 1, 1, 2, 12, attr, strlen(attr)+1);
	if (ret != 0)
		osd_error_errno("%s: attr_set_attr failed", __func__);

	void *val = Calloc(1, 1024);
	if (!val)
		osd_error_errno("%s: Calloc failed", __func__);
	ret = attr_get_attr(osd->db, 1, 1, 2, 12, val, 1024);
	if (ret != 0)
		osd_error_errno("%s: attr_get_attr failed", __func__);
	list_entry_t *ent = (list_entry_t *)val;
	printf("retreived: %lu %u %u %u %s\n", 1UL, 
	       ntohl_le((uint8_t *)&ent->page), 
	       ntohl_le((uint8_t *)&ent->number), 
	       ntohs_le((uint8_t *)&ent->len), 
	       (uint8_t *)ent + ATTR_VAL_OFFSET); 

	/* get non-existing attr */
	ret = attr_get_attr(osd->db, 2, 1, 2, 12, val, 1024);
	if (ret != 0)
		osd_error("%s: attr_get_attr failed as expected", __func__);

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
			osd_error_errno("%s: obj_insert failed", __func__);
	}

	ret = obj_get_nextoid(osd->db, 1, &oid);
	if (ret != 0)
		osd_error_errno("%s: obj_get_nextoid failed", __func__);
	printf("%s: next oid = %llu\n", __func__, llu(oid));	

	/* get nextoid for new (pid, oid) */
	ret = obj_get_nextoid(osd->db, 4, &oid);
	if (ret != 0)
		osd_error_errno("%s: obj_get_nextoid failed", __func__);
	printf("%s: next oid = %llu\n", __func__, llu(oid));	

	for (i =0; i < 4; i++) {
		ret = obj_delete(osd->db, 1, 1<<i);
		if (ret != 0)
			osd_error_errno("%s: obj_delete failed", __func__);
	}
	
	ret = obj_insert(osd->db, 1, 235, USEROBJECT);
	if (ret != 0)
		osd_error_errno("%s: obj_insert failed", __func__);

	present = obj_ispresent(osd->db, 1, 235);
	printf("obj_ispresent = %d\n", present);

	ret = obj_delete(osd->db, 1, 235);
	if (ret != 0)
		osd_error_errno("%s: obj_delete failed", __func__);

	present = obj_ispresent(osd->db, 1, 235);
	printf("obj_ispresent = %d\n", present);
}

void test_pid_isempty(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 1, USEROBJECT);
	if (ret != 0)
		osd_error_fatal("obj_insert failed");

	ret = obj_pid_isempty(osd->db, 1);
	osd_debug("full pid ret %d", ret);

	ret = obj_delete(osd->db, 1, 1);
	if (ret != 0)
		osd_error_fatal("obj_delete failed");

	ret = obj_pid_isempty(osd->db, 1);
	osd_debug("empty pid ret %d", ret);
}

void test_get_obj_type(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 1, USEROBJECT);
	if (ret != 0)
		osd_error_fatal("obj_insert failed");

	ret = obj_insert(osd->db, 2, 2, COLLECTION);
	if (ret != 0)
		osd_error_fatal("obj_insert failed");

	ret = obj_insert(osd->db, 3, 0, PARTITION);
	if (ret != 0)
		osd_error_fatal("obj_insert failed");

	ret = obj_get_type(osd->db, 0, 0);
	osd_debug("type of root %d", ret);
	ret = obj_get_type(osd->db, 1, 1);
	osd_debug("type of userobject %d", ret);
	ret = obj_get_type(osd->db, 2, 2);
	osd_debug("type of collection %d", ret);
	ret = obj_get_type(osd->db, 3, 0);
	osd_debug("type of partition %d", ret);

	ret = obj_delete(osd->db, 3, 0);
	if (ret)
		osd_error_fatal("deletion of 3, 0 failed");
	ret = obj_delete(osd->db, 2, 2);
	if (ret)
		osd_error_fatal("deletion of 2, 2 failed");
	ret = obj_delete(osd->db, 1, 1);
	if (ret)
		osd_error_fatal("deletion of 1, 1 failed");

	ret = obj_get_type(osd->db, 1, 1);
	osd_debug("type of non existing object %d", ret);
}

void test_osd_interface(void)
{
	int ret = 0;
	const char *root = "/tmp/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	if (ret != 0)
		osd_error_errno("%s: osd_open", __func__);

	test_obj(&osd);
	test_attr(&osd);

	ret = osd_close(&osd);
	if (ret != 0)
		osd_error_errno("%s: osd_close", __func__);
}

int main()
{
	char path[]="/tmp/osd/osd.db";
	int ret = 0;
	struct osd_device osd;

	ret = db_open(path, &osd);
	if (ret != 0)
		return ret;

	test_obj(&osd);
	test_attr(&osd);
	test_dup_obj(&osd);
	test_obj_manip(&osd);
	test_pid_isempty(&osd);
	test_get_obj_type(&osd);

	ret = db_close(&osd);
	if (ret != 0)
		return ret;

	/*test_osd_interface();*/

	return 0;
}
