#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "osd-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util/util.h"
#include "cdb-manip.h"
#include "cdb.h"
#include "osd-sense.h"

#define CDB_SZ (200)

void test_partition(struct osd_device *osd);
void test_create(struct osd_device *osd);

void test_partition(struct osd_device *osd) 
{
	int ret = 0;
	uint8_t cdb[CDB_SZ];
	int senselen_out;
	uint8_t sense_out[MAX_SENSE_LEN];
	uint8_t *data_out;
	uint64_t data_out_len;

	/* create partition + empty getpage_setlist */
	ret = set_cdb_osd_create_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* remove partition + empty getpage_setlist */
	ret = set_cdb_osd_remove_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* create partition + empty getlist_setlist */
	ret = set_cdb_osd_create_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = set_cdb_getlist_setlist(cdb, 0, 0, 0, 0, 0, 0);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* remove partition + empty getpage_setlist */
	ret = set_cdb_osd_remove_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = set_cdb_getlist_setlist(cdb, 0, 0, 0, 0, 0, 0);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* create partition, get ccap, set value */
/*	ret = set_cdb_osd_create_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = set_cdb_getpage_set*/
}

void test_create(struct osd_device *osd)
{
	int ret = 0;
	uint8_t cdb[CDB_SZ];
	int senselen_out;
	uint8_t sense_out[MAX_SENSE_LEN];
	uint8_t *data_out;
	uint64_t data_out_len;

	/* create partition + empty getpage_setlist */
	ret = set_cdb_osd_create_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* create 1 object */
	ret = set_cdb_osd_create(cdb, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 1);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* remove the object */
	ret = set_cdb_osd_remove(cdb, USEROBJECT_PID_LB, USEROBJECT_OID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* create 5 objects & get ccap */
	ret = set_cdb_osd_create(cdb, USEROBJECT_PID_LB, 0, 5);
	assert(ret == 0);
	ret = set_cdb_getpage_setvalue(cdb, CUR_CMD_ATTR_PG, CCAP_TOTAL_LEN,
				       0, 0, 0, 0, 0); 
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	assert(ntohl_le(&data_out[CCAP_PAGEID_OFF]) == CUR_CMD_ATTR_PG);
	assert(ntohl_le(&data_out[CCAP_LEN_OFF]) == CCAP_LEN);
	assert(data_out[CCAP_OBJT_OFF] == USEROBJECT);
	assert(ntohll_le(&data_out[CCAP_PID_OFF]) == USEROBJECT_PID_LB);
	assert(ntohll_le(&data_out[CCAP_APPADDR_OFF]) == 0);
	uint64_t i = ntohll_le(&data_out[CCAP_OID_OFF]);
	assert (i == (USEROBJECT_PID_LB + 5 - 1));

	i -= (5-1);
	/* remove 5 objects */
	for (;i < USEROBJECT_OID_LB + 5; i++) {
		ret = set_cdb_osd_remove(cdb, USEROBJECT_PID_LB, i);
		assert(ret == 0);
		ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out,
					&data_out_len, sense_out,
					&senselen_out);
		assert(ret == 0);
	}

	/* create 5 objects, set 2 attr on each */
	char str1[MAXNAMELEN], str2[MAXNAMELEN];
	sprintf(str1, "Madhuri Dixit Rocks!!");
	sprintf(str2, "A ciggarate a day, kills a moron anyway.");
	struct list_entry le[] = {
		{USEROBJECT_PG+LUN_PG_LB, 111, strlen(str1)+1, str1}, 
		{USEROBJECT_PG+LUN_PG_LB+1, 321, strlen(str2)+1, str2}
	};

	ret = set_cdb_osd_create(cdb, USEROBJECT_PID_LB, 0, 5);
	assert(ret == 0);

	uint8_t indata[1024];
	int len = set_cdb_setattr_list(&indata[512], le, 2);
	assert(len > 0);

	ret = set_cdb_getlist_setlist(cdb, 0, 0, 0, 0, len, 512);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, indata, 1024, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* remove 5 objects and get previously set attributes for each */
	struct getattr_list_entry gl[] = { 
		{USEROBJECT_PG+LUN_PG_LB+1, 321}, 
		{USEROBJECT_PG+LUN_PG_LB, 111} 
	};
	for (i = USEROBJECT_OID_LB; i < (USEROBJECT_OID_LB + 5); i++) {
		uint8_t *cp = NULL;
		uint8_t pad = 0;

		ret = set_cdb_osd_remove(cdb, USEROBJECT_PID_LB, i);
		assert(ret == 0);

		len = set_cdb_getattr_list(&indata[256], gl, 2);
		assert(len > 0);
		ret = set_cdb_getlist_setlist(cdb, len, 256, 1024, 512, 0, 0);
		assert(ret == 0);
		ret = osdemu_cmd_submit(osd, cdb, indata, 1024, &data_out,
					&data_out_len, sense_out,
					&senselen_out);
		assert(ret == 0);
		assert(data_out[512] == RTRVD_SET_ATTR_LIST);
		len = ntohl(&data_out[512+4]);
		assert(len > 0);
		cp = &data_out[512+8];
		assert(ntohl(&cp[LE_PAGE_OFF]) == USEROBJECT_PG+LUN_PG_LB+1);
		assert(ntohl(&cp[LE_NUMBER_OFF]) == 321);
		len = ntohs(&cp[LE_LEN_OFF]);
		assert((uint32_t)len == (strlen(str2)+1));
		assert(memcmp(&cp[LE_VAL_OFF], str2, len) == 0);
		cp += len + LE_VAL_OFF;
		pad = (0x8 - ((uint64_t)cp & 0x7)) & 0x7;
		while (pad--)
			assert(*cp == 0), cp++;
		assert(ntohl(&cp[LE_PAGE_OFF]) == USEROBJECT_PG+LUN_PG_LB);
		assert(ntohl(&cp[LE_NUMBER_OFF]) == 111);
		len = ntohs(&cp[LE_LEN_OFF]);
		assert((uint32_t)len == (strlen(str1)+1));
		assert(memcmp(&cp[LE_VAL_OFF], str1, len) == 0);
		cp += len + LE_VAL_OFF;
		pad = (0x8 - ((uint64_t)cp & 0x7)) & 0x7;
		while (pad--)
			assert(*cp == 0), cp++;
	}

	/* remove partition */
	ret = set_cdb_osd_remove_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);
}

int main()
{
	int ret = 0;
	const char *root = "/tmp/osd/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	assert(ret == 0);

	test_partition(&osd);
	test_create(&osd);

	ret = osd_close(&osd);
	assert(ret == 0);

	return 0;
}
