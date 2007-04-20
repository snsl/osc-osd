#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/osd-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util/util.h"
#include "cdb.h"
#include "util/osd-sense.h"
#include "osd_initiator/command.h"

void test_partition(struct osd_device *osd);
void test_create(struct osd_device *osd);

void test_partition(struct osd_device *osd) 
{
	int ret = 0;
	struct osd_command cmd;
	int senselen_out;
	uint8_t sense_out[OSD_MAX_SENSE];
	uint8_t *data_out;
	const void *data_in;
	uint64_t data_out_len, data_in_len;

	/* create partition + empty getpage_setlist */
	ret = osd_command_set_create_partition(&cmd, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* remove partition + empty getpage_setlist */
	memset(&cmd, 0, sizeof(cmd));
	ret = osd_command_set_remove_partition(&cmd, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* create partition + empty getlist_setlist */
	struct attribute_list attr = {ATTR_GET_PAGE, 0, 0, NULL, 0, 0};

	ret = osd_command_set_create_partition(&cmd, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, &attr, 1);
	assert (ret == 0);
	data_in = cmd.outdata;
	data_in_len = cmd.outlen;
	ret = osdemu_cmd_submit(osd, cmd.cdb, cmd.outdata, cmd.outlen,
				&data_out, &data_out_len, sense_out,
				&senselen_out);
	assert(ret == 0);

	/* remove partition + empty getpage_setlist */
	ret = osd_command_set_remove_partition(&cmd, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, &attr, 1);
	assert (ret == 0);
	data_in = cmd.outdata;
	data_in_len = cmd.outlen;
	ret = osdemu_cmd_submit(osd, cmd.cdb, cmd.outdata, cmd.outlen,
				&data_out, &data_out_len, sense_out,
				&senselen_out);
	assert(ret == 0);

	free(cmd.attr_malloc);

}

void test_create(struct osd_device *osd)
{
	int ret = 0;
	struct osd_command cmd;
	int senselen_out;
	uint8_t sense_out[OSD_MAX_SENSE];
	uint8_t *data_out;
	const void *data_in;
	uint64_t data_out_len, data_in_len;

	/* create partition + empty getpage_setlist */
	ret = osd_command_set_create_partition(&cmd, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* create 1 object */
	ret = osd_command_set_create(&cmd, USEROBJECT_PID_LB,
				     USEROBJECT_OID_LB, 1); 
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	/* remove the object */
	ret = osd_command_set_remove(&cmd, USEROBJECT_PID_LB, 
				     USEROBJECT_OID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);

	struct attribute_list attr = {
		ATTR_GET_PAGE, CUR_CMD_ATTR_PG, 0, NULL, CCAP_TOTAL_LEN, 0
	};
	/* create 5 objects & get ccap */
	ret = osd_command_set_create(&cmd, USEROBJECT_PID_LB, 0, 5);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, &attr, 1);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out, 
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	assert(ntohl_le(&data_out[0]) == CUR_CMD_ATTR_PG);
	assert(ntohl_le(&data_out[4]) == CCAP_TOTAL_LEN - 8);
	assert(data_out[CCAP_OBJT_OFF] == USEROBJECT);
	assert(ntohll_le(&data_out[CCAP_PID_OFF]) == USEROBJECT_PID_LB);
	assert(ntohll_le(&data_out[CCAP_APPADDR_OFF]) == 0);
	uint64_t i = ntohll_le(&data_out[CCAP_OID_OFF]);
	assert (i == (USEROBJECT_PID_LB + 5 - 1));

	i -= (5-1);
	/* remove 5 objects */
	for (;i < USEROBJECT_OID_LB + 5; i++) {
		ret = osd_command_set_remove(&cmd, USEROBJECT_PID_LB, i);
		assert(ret == 0);
		ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
					&data_out_len, sense_out,
					&senselen_out);
		assert(ret == 0);
	}

	/* create 5 objects, set 2 attr on each */
	char str1[MAXNAMELEN], str2[MAXNAMELEN];
	sprintf(str1, "Madhuri Dixit Rocks!!");
	sprintf(str2, "A ciggarate a day, kills a moron anyway.");
	struct attribute_list setattr[] = {
		{ATTR_SET, USEROBJECT_PG+LUN_PG_LB, 111, str1, strlen(str1)+1, 
			0
		}, 
		{ATTR_SET, USEROBJECT_PG+LUN_PG_LB+1, 321, str2,
			strlen(str2)+1, 0
		} 
	};

	ret = osd_command_set_create(&cmd, USEROBJECT_PID_LB, 0, 5);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, setattr, 2);
	assert(ret == 0);
	data_in = cmd.outdata;
	data_in_len = cmd.outlen;
	ret = osdemu_cmd_submit(osd, cmd.cdb, data_in, data_in_len, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* remove 5 objects and get previously set attributes for each */
	struct attribute_list getattr[] = { 
		{ATTR_GET, USEROBJECT_PG+LUN_PG_LB+1, 321, NULL, strlen(str2), 
			0
		}, 
		{ATTR_GET, USEROBJECT_PG+LUN_PG_LB, 111, NULL, strlen(str1),
			0
		} 
	};
	for (i = USEROBJECT_OID_LB; i < (USEROBJECT_OID_LB + 5); i++) {
		uint8_t *cp = NULL;
		uint8_t pad = 0;
		uint32_t len = 0;

		ret = osd_command_set_remove(&cmd, USEROBJECT_PID_LB, i);
		assert(ret == 0);
		ret = osd_command_attr_build(&cmd, getattr, 2);
		assert(ret == 0);
		data_in = cmd.outdata;
		data_in_len = cmd.outlen;
		ret = osdemu_cmd_submit(osd, cmd.cdb, data_in, data_in_len,
					&data_out, &data_out_len, sense_out,
					&senselen_out);
		assert(ret == 0);
		assert(data_out[0] == RTRVD_SET_ATTR_LIST);
		len = ntohl(&data_out[4]);
		assert(len > 0);
		cp = &data_out[8];
		assert(ntohl(&cp[LE_PAGE_OFF]) == USEROBJECT_PG+LUN_PG_LB+1);
		assert(ntohl(&cp[LE_NUMBER_OFF]) == 321);
		len = ntohs(&cp[LE_LEN_OFF]);
		assert((uint32_t)len == (strlen(str2)+1));
		assert(memcmp(&cp[LE_VAL_OFF], str2, len) == 0);
		cp += len + LE_VAL_OFF;
		pad = (0x8 - ((uintptr_t)cp & 0x7)) & 0x7;
		while (pad--)
			assert(*cp == 0), cp++;
		assert(ntohl(&cp[LE_PAGE_OFF]) == USEROBJECT_PG+LUN_PG_LB);
		assert(ntohl(&cp[LE_NUMBER_OFF]) == 111);
		len = ntohs(&cp[LE_LEN_OFF]);
		assert((uint32_t)len == (strlen(str1)+1));
		assert(memcmp(&cp[LE_VAL_OFF], str1, len) == 0);
		cp += len + LE_VAL_OFF;
		pad = (0x8 - ((uintptr_t)cp & 0x7)) & 0x7;
		while (pad--)
			assert(*cp == 0), cp++;
	}

	/* remove partition */
	ret = osd_command_set_remove_partition(&cmd, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	free(cmd.attr_malloc);
}

int main()
{
	int ret = 0;
	const char *root = "/tmp/osd/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	assert(ret == 0);

	/* test_partition(&osd); */
	test_create(&osd);

	ret = osd_close(&osd);
	assert(ret == 0);

	return 0;
}
