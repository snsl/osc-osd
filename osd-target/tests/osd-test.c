#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/osd-defs.h"
#include "target-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util/util.h"
#include "util/osd-sense.h"
#include "target-sense.h"

void test_osd_create(struct osd_device *osd);
void test_osd_set_attributes(struct osd_device *osd);
void test_osd_format(struct osd_device *osd);
void test_osd_read_write(struct osd_device *osd);
void test_osd_create_partition(struct osd_device *osd);
void test_osd_get_ccap(struct osd_device *osd, uint64_t pid, uint64_t oid);
void test_osd_get_utsap(struct osd_device *osd);
void test_osd_get_attributes(struct osd_device *osd);

void test_osd_create(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	/* invalid pid/oid, test must fail */
	ret = osd_create(osd, 0, 1, 0, sense);
	assert(ret != 0);

	/* invalid oid test must fail */
	ret = osd_create(osd, USEROBJECT_PID_LB, 1, 0, sense);
	assert(ret != 0);

	/* num > 1 cannot request oid, test must fail */
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 2, sense);
	assert(ret != 0);

	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	assert(ret == 0);

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	assert(ret == 0);

	/* remove non-existing object, test must fail succeed */
	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	assert(ret != 0);

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	free(sense);
}

void test_osd_set_attributes(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);
	void *val = Calloc(1, 1024);

	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	assert(ret == 0);

	/* setting root attr must fail */
	ret = osd_set_attributes(osd, ROOT_PID, ROOT_OID, 0, 0, 
				 NULL, 0, TRUE, sense);
	assert(ret != 0);

	/* unsettable page modification must fail */
	ret = osd_set_attributes(osd, PARTITION_PID_LB, PARTITION_OID, 0, 0, 
				 NULL, 0, TRUE, sense);
	assert(ret != 0);

	/* unsettable collection page must fail */
	ret = osd_set_attributes(osd, COLLECTION_PID_LB, COLLECTION_OID_LB, 0, 
				 0, NULL, 0, TRUE, sense);
	assert(ret != 0);

	/* unsettable userobject page must fail */
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0,
				 0, NULL, 0, TRUE, sense);
	assert(ret != 0);

	/* info attr < 40 bytes, test must fail */
	sprintf(val, "This is test, long test more than forty bytes");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG + LUN_PG_LB, ATTRNUM_INFO, val, 
				 strlen(val)+1, TRUE, sense);
	assert(ret != 0);

	/* this test is normal setattr, must succeed */
	sprintf(val, "Madhuri Dixit");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG + LUN_PG_LB, 1, val, 
				 strlen(val)+1, TRUE, sense);
	assert(ret == 0);

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	assert(ret == 0);

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	free(sense);
	free(val);
}

void test_osd_format(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_format_osd(osd, 0, sense);
	assert(ret == 0);

	free(sense);
}

void test_osd_read_write(struct osd_device *osd)
{
	int ret = 0;
	uint8_t *sense = Calloc(1, 1024);
	void *mybuf = Calloc(1, 256);
	uint8_t *buf = Calloc(1, 256);
	uint64_t len;

	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	assert(ret == 0);

	sprintf(mybuf, "Hello World! Get life\n");
	ret = osd_write(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
			strlen(mybuf)+1, 0, mybuf, sense);
	assert(ret == 0);

	ret = osd_read(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
		       256, 0, buf, &len, sense);
	assert(ret >= 0);
	if (ret > 0) {
		assert(sense_test_type(sense, OSD_SSK_RECOVERED_ERROR,
				       OSD_ASC_READ_PAST_END_OF_USER_OBJECT));
		assert(ntohll(&sense[44]) == len);
	}
	assert(len == strlen(mybuf)+1);
	assert(strcmp((void *) buf, mybuf) == 0);

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	assert(ret == 0);

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);
		
	free(buf);
	free(sense);
	free(mybuf);
}

void test_osd_create_partition(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_create_partition(osd, 0, sense);
	assert(ret == 0);

	/* create dup pid must fail */
	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	assert(ret != 0);

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	/* remove non-existing object, test must succeed: sqlite semantics */
	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	free(sense);
}

void test_osd_get_ccap(struct osd_device *osd, uint64_t pid, uint64_t oid)
{
	int ret = 0, i = 0;
	void *sense = Calloc(1, 1024);
	void *val = Calloc(1, 1024);
	uint8_t *cp = NULL;
	uint32_t used_len = 0;
	uint64_t len = 0;

	len = 1024;
	used_len = 0;
	ret = osd_getattr_page(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB,
			       CUR_CMD_ATTR_PG, val, len, TRUE, &used_len,
			       sense);
	assert(ret == 0);
	assert(used_len == CCAP_TOTAL_LEN);

	cp = val;
	assert(ntohl_le(&cp[0]) == CUR_CMD_ATTR_PG);
	assert(ntohl_le(&cp[4]) == CCAP_TOTAL_LEN - 8);
	for (i = CCAP_RICV_OFF; i < CCAP_RICV_OFF + CCAP_RICV_LEN; i++)
		assert(cp[i] == 0);
	assert(cp[CCAP_OBJT_OFF] == USEROBJECT);
	assert(ntohll_le(&cp[CCAP_PID_OFF]) == pid);
	assert(ntohll_le(&cp[CCAP_OID_OFF]) == oid);
	assert(ntohll_le(&cp[CCAP_APPADDR_OFF]) == 0);

	free(sense);
	free(val);
}

static inline time_t ntoh_time(void *buf)
{
	time_t t = ntohll_le((uint8_t *)buf) & ~0xFFFFULL;
	return (t >> 16);
}

void test_osd_get_utsap(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);
	void *buf = Calloc(1, 1024);
	uint8_t *cp = NULL;
	uint32_t used_getlen = 0;
	uint64_t used_len = 0;
	uint64_t len = 0;

	sprintf(buf, "Hello World! Get life blah blah blah\n");
	ret = osd_write(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
			strlen(buf)+1, 0, buf, sense);
	assert(ret == 0);

	sleep(1);
	used_len = 0;
	ret = osd_read(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB,
		       strlen(buf)+1, 0, buf, &used_len, sense);
	assert(ret == 0);
	assert(used_len = strlen(buf)+1);

	/*sleep(1);*/
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG + LUN_PG_LB, 2, buf, 
				 strlen(buf)+1, TRUE, sense);
	assert(ret == 0);

	len = 1024;
	used_getlen = 0;
	ret = osd_getattr_page(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB,
			       USER_TMSTMP_PG, buf, len, FALSE, &used_getlen,
			       sense); 
	assert(ret == 0);

	cp = buf;
	assert(ntohl_le(&cp[0]) == USER_TMSTMP_PG);
	assert(ntohl_le(&cp[4]) == UTSAP_TOTAL_LEN - 8);

	time_t atime = ntoh_time(&cp[UTSAP_DATA_ATIME_OFF]);
	time_t mtime = ntoh_time(&cp[UTSAP_DATA_MTIME_OFF]);
	assert(atime != 0 && mtime != 0 && mtime < atime);

	atime = ntoh_time(&cp[UTSAP_ATTR_ATIME_OFF]);
	mtime = ntoh_time(&cp[UTSAP_ATTR_MTIME_OFF]);
	assert(atime != 0 && mtime != 0 && mtime == atime);
}

void test_osd_get_attributes(struct osd_device *osd)
{
	int ret = 0;
	uint32_t used_len = 0;
	uint64_t len = 0;
	void *sense = Calloc(1, 1024);
	void *val = Calloc(1, 1024);
	struct list_entry *le = NULL;

	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	assert(ret == 0);

	sprintf(val, "Madhuri Dixit");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG + LUN_PG_LB, 1, val, 
				 strlen(val)+1, TRUE, sense);
	assert(ret == 0);

	len = 1024;
	used_len = 0;
	ret = osd_getattr_list(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB,
			       USEROBJECT_PG + LUN_PG_LB, 1, val, len, TRUE,
			       RTRVD_SET_ATTR_LIST, &used_len, sense);
	assert(ret == 0);
	le = val;
	assert(ntohl_le((uint8_t *)&le->page) == USEROBJECT_PG + LUN_PG_LB);
	assert(ntohl_le((uint8_t *)&le->number) == 1);
	assert(ntohs_le((uint8_t *)&le->len) == strlen("Madhuri Dixit")+1);
	assert(strcmp((char *)le +  LE_VAL_OFF, "Madhuri Dixit") == 0);
	len = LE_VAL_OFF + strlen("Madhuri Dixit")+1;
	len += (0x8 - (len & 0x7)) & 0x7;
	assert(used_len == len);

	/* run different get attr tests */
	test_osd_get_ccap(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB); 
	test_osd_get_utsap(osd);

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	assert(ret == 0);

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	assert(ret == 0);

	free(sense);
	free(val);
}

int main()
{
	int ret = 0;
	const char *root = "/tmp/osd/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	assert(ret == 0);

	test_osd_create(&osd);
	test_osd_set_attributes(&osd);
	test_osd_format(&osd);
	test_osd_read_write(&osd);
	test_osd_create_partition(&osd);
	test_osd_get_attributes(&osd);

	ret = osd_close(&osd);
	assert(ret == 0);

	return 0;
}

