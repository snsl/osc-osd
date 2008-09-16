#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "osd-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util/util.h"

void test_obj(struct osd_device *osd);
void test_dup_obj(struct osd_device *osd);
void test_obj_manip(struct osd_device *osd);
void test_pid_isempty(struct osd_device *osd);
void test_get_obj_type(struct osd_device *osd);
void test_attr(struct osd_device *osd);
void test_dir_page(struct osd_device *osd);

void test_obj(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	assert(ret == 0);

	ret = obj_delete(osd->db, 1, 2);
	assert(ret == 0);
}

void test_dup_obj(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	assert(ret == 0);

	/* duplicate insert must fail */
	ret = obj_insert(osd->db, 1, 2, USEROBJECT);
	assert(ret != 0);

	ret = obj_delete(osd->db, 1, 2);
	assert(ret == 0);
}

void test_attr(struct osd_device *osd)
{
	int ret= 0;
	const char *attr = "This is first attr";
	uint32_t len = 0;
	uint8_t listfmt = 0;

	ret = attr_set_attr(osd->db, 1, 1, 2, 12, attr, strlen(attr)+1);
	assert(ret == 0);

	void *val = Calloc(1, 1024);
	if (!val)
		osd_error_errno("%s: Calloc failed", __func__);

	listfmt = RTRVD_SET_ATTR_LIST;
	ret = attr_get_attr(osd->db, 1, 1, 2, 12, val, 1024, listfmt, &len);
	assert(ret == 0);
	uint32_t l = strlen(attr)+1+LE_VAL_OFF;
	l += (0x8 - (l & 0x7)) & 0x7;
	assert(len == l);
	struct list_entry *ent = (struct list_entry *)val;
	assert(ntohl_le((uint8_t *)&ent->page) == 2);
	assert(ntohl_le((uint8_t *)&ent->number) == 12);
	assert(ntohs_le((uint8_t *)&ent->len) == strlen(attr)+1);
	assert(strcmp((char *)ent + LE_VAL_OFF, attr) == 0); 

	/* get non-existing attr, must fail */
	ret = attr_get_attr(osd->db, 2, 1, 2, 12, val, 1024, listfmt, &len);
	assert(ret != 0);

	free(val);
}

void test_obj_manip(struct osd_device *osd)
{
	int i = 0;
	int ret = 0;
	uint64_t oid = 0;

	for (i =0; i < 4; i++) {
		ret = obj_insert(osd->db, 1, 1<<i, USEROBJECT);
		assert(ret == 0);
	}

	ret = obj_get_nextoid(osd->db, 1, &oid);
	assert(ret == 0);
	/* nextoid for partition 1 is 9 */
	assert(oid == 9);

	/* get nextoid for new (pid, oid) */
	ret = obj_get_nextoid(osd->db, 4, &oid);
	assert(ret == 0);
	/* nextoid for new partition == 1 */
	assert(oid == 1);

	for (i =0; i < 4; i++) {
		ret = obj_delete(osd->db, 1, 1<<i);
		assert(ret == 0);
	}
	
	ret = obj_insert(osd->db, 1, 235, USEROBJECT);
	assert(ret == 0);

	/* existing object, ret == 1 */
	ret = obj_ispresent(osd->db, 1, 235);
	assert(ret == 1);

	ret = obj_delete(osd->db, 1, 235);
	assert(ret == 0);

	/* non-existing object, ret == 0 */
	ret = obj_ispresent(osd->db, 1, 235);
	assert(ret == 0);
}

void test_pid_isempty(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 1, USEROBJECT);
	assert(ret == 0);

	/* pid is not empty, ret should be 0 */
	ret = obj_pid_isempty(osd->db, 1);
	assert(ret == 0);

	ret = obj_delete(osd->db, 1, 1);
	assert(ret == 0);

	/* pid is empty, ret should be 1 */
	ret = obj_pid_isempty(osd->db, 1);
	assert(ret == 1);
}

void test_get_obj_type(struct osd_device *osd)
{
	int ret = 0;

	ret = obj_insert(osd->db, 1, 1, USEROBJECT);
	assert(ret == 0);

	ret = obj_insert(osd->db, 2, 2, COLLECTION);
	assert(ret == 0);

	ret = obj_insert(osd->db, 3, 0, PARTITION);
	assert(ret == 0);

	ret = obj_get_type(osd->db, 0, 0);
	assert(ret == ROOT);

	ret = obj_get_type(osd->db, 1, 1);
	assert(ret == USEROBJECT);

	ret = obj_get_type(osd->db, 2, 2);
	assert(ret == COLLECTION);

	ret = obj_get_type(osd->db, 3, 0);
	assert(ret == PARTITION);

	ret = obj_delete(osd->db, 3, 0);
	assert(ret == 0);

	ret = obj_delete(osd->db, 2, 2);
	assert(ret == 0);

	ret = obj_delete(osd->db, 1, 1);
	assert(ret == 0);

	/* non-existing object's type must be ILLEGAL_OBJ */
	ret = obj_get_type(osd->db, 1, 1);
	assert(ret == ILLEGAL_OBJ);
}

static inline void delete_obj(struct osd_device *osd, uint64_t pid, 
			      uint64_t oid)
{
	int ret = 0;
	ret = obj_delete(osd->db, pid, oid);
	assert (ret == 0);
	ret = attr_delete_all(osd->db, pid, oid);
	assert (ret == 0);
}

void test_dir_page(struct osd_device *osd)
{
	int ret= 0;
	uint8_t buf[1024];
	char pg3[] = "OSC     page 3 id                      ";
	char pg4[] = "OSC     page 4 id                      ";
	char uid[] = "        unidentified attributes page   ";
	uint32_t used_len;
	uint32_t val;

	delete_obj(osd, 1, 1);

	ret = obj_insert(osd->db, 1, 1, USEROBJECT);
	assert(ret == 0);

	val = 44;
	ret = attr_set_attr(osd->db, 1, 1, 1, 2, &val, sizeof(val));
	assert(ret == 0);
	val = 444;
	ret = attr_set_attr(osd->db, 1, 1, 1, 3, &val, sizeof(val));
	assert(ret == 0);
	val = 333;
	ret = attr_set_attr(osd->db, 1, 1, 2, 22, &val, sizeof(val));
	assert(ret == 0);
	ret = attr_set_attr(osd->db, 1, 1, 3, 0, pg3, sizeof(pg3));
	assert(ret == 0);
	val = 321;
	ret = attr_set_attr(osd->db, 1, 1, 3, 3, &val, sizeof(val));
	assert(ret == 0);
	ret = attr_set_attr(osd->db, 1, 1, 4, 0, pg4, sizeof(pg4));
	assert(ret == 0);

	memset(buf, 0, sizeof(buf));
	ret = attr_get_dir_page(osd->db, 1, 1, USEROBJECT_DIR_PG, buf, 
				sizeof(buf), RTRVD_SET_ATTR_LIST, &used_len);
	assert (ret == 0);

	uint8_t *cp = buf;
	int i = 0;
	for (i = 1; i <= 4; i++) {
		uint8_t pad = 0;
		uint16_t len = 0;
		uint32_t j = 0;
		assert(ntohl(&cp[LE_PAGE_OFF]) == USEROBJECT_DIR_PG);
		j = ntohl(&cp[LE_NUMBER_OFF]);
		assert(j == (uint32_t)i);
		len = ntohs(&cp[LE_LEN_OFF]);
		assert(len == 40);
		cp += 10;
		if (i == 1 || i == 2)
			assert(strcmp((char *)cp, uid) == 0);
		else if (i == 3)
			assert(strcmp((char *)cp, pg3) == 0);
		else if (i == 4)
			assert(strcmp((char *)cp, pg4) == 0);
		cp += len;
		pad = (0x8 - ((10+len) & 0x7)) & 0x7;
		while (pad--)
			assert(*cp == 0), cp++;
	}

	delete_obj(osd, 1, 1);
}

int main()
{
	char path[]="/tmp/osd/osd.db";
	int ret = 0;
	struct osd_device osd;

	ret = db_open(path, &osd);
	assert(ret == 0);

/*	test_obj(&osd);
	test_dup_obj(&osd);
	test_obj_manip(&osd);
	test_pid_isempty(&osd);
	test_get_obj_type(&osd);
	test_attr(&osd);*/
	test_dir_page(&osd);

	ret = db_close(&osd);
	assert(ret == 0);

	return 0;
}
