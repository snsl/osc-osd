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
void test_list(struct osd_device *osd);

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

/* only to be used by test_osd_query */
static void set_attr_int(struct osd_device *osd, uint64_t pid, uint64_t oid, 
			 uint32_t page, uint32_t number, uint64_t val)
{
	struct osd_command cmd;
	uint8_t sense_out[OSD_MAX_SENSE];
	int senselen_out;
	uint8_t *data_out;
	uint64_t data_out_len;
	uint64_t attrval;
	int ret;
	struct attribute_list attr = {
	    .type = ATTR_SET,
	    .page = page,
	    .number = number,
	    .len = 8,
	    .val = &attrval,
	};

	set_htonll((uint8_t *) &attrval, val);
	ret = osd_command_set_set_attributes(&cmd, pid, oid);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, &attr, 1);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, cmd.outdata, cmd.outlen,
				&data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);
	osd_command_attr_free(&cmd);
}

static void set_attr_val(struct osd_device *osd, uint64_t pid, uint64_t oid,
			 uint32_t page, uint32_t number, const void *val,
			 uint16_t len)
{
	struct osd_command cmd;
	uint8_t sense_out[OSD_MAX_SENSE];
	int senselen_out;
	uint8_t *data_out;
	uint64_t data_out_len;
	int ret;
	struct attribute_list attr = {
	    .type = ATTR_SET,
	    .page = page,
	    .number = number,
	    .len = len,
	    .val = (void *)(uintptr_t) val,
	};

	ret = osd_command_set_set_attributes(&cmd, pid, oid);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, &attr, 1);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, cmd.outdata, cmd.outlen,
				&data_out, &data_out_len,
				sense_out, &senselen_out);
	assert(ret == 0);
	osd_command_attr_free(&cmd);
}

static void set_qce(uint8_t *cp, uint32_t page, uint32_t number, 
		    uint16_t min_len, const void *min_val, 
		    uint16_t max_len, const void *max_val)
{
	uint16_t len = 4 + 4 + 2 + min_len + 2 + max_len;

	set_htons(&cp[2], len);
	set_htonl(&cp[4], page);
	set_htonl(&cp[8], number);
	set_htons(&cp[12], min_len);
	memcpy(&cp[14], min_val, min_len);
	set_htons(&cp[14+min_len], max_len);
	memcpy(&cp[16+min_len], max_val, max_len);
}

static int ismember(uint64_t needle, uint64_t *hay, uint64_t haysz)
{

	while (haysz--)
		if (needle == hay[haysz])
			return 1;
	return 0;
}

static void check_results(uint8_t *matches, uint64_t matchlen,
                          uint64_t *idlist, uint64_t idlistlen)
{
	uint32_t add_len = ntohll(&matches[0]);

	assert(add_len == (5+8*idlistlen));
	assert(matches[12] == (0x21 << 2));
	assert(matchlen == add_len+8);
	add_len -= 5;
	matches += MIN_ML_LEN;
	while (add_len) {
		assert(ismember(ntohll(matches), idlist, 8));
		matches += 8;
		add_len -= 8;
	}
}

static void test_query(struct osd_device *osd)
{
	struct osd_command cmd;
	uint64_t pid = PARTITION_PID_LB;
	uint64_t cid = COLLECTION_OID_LB;
	uint64_t oid = USEROBJECT_OID_LB + 1;  /* leave room for cid */
	uint8_t *data_out;
	uint64_t data_out_len;
	uint8_t sense_out[OSD_MAX_SENSE];
	int senselen_out;
	int i, ret;

	/* create a collection and stick some objects in it */
	ret = osd_command_set_create_partition(&cmd, pid);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);
	ret = osd_command_set_create_collection(&cmd, pid, cid);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* but don't put all of the objects in the collection */
	for (i=0; i<10; i++) {
		uint64_t attrval;
		struct attribute_list attr = {
		    .type = ATTR_SET,
		    .page = COLLECTIONS_PG,
		    .number = 1,
		    .len = 8,
		    .val = &attrval,
		};
		set_htonll((uint8_t *) &attrval, cid);
		ret = osd_command_set_create(&cmd, pid, oid + i, 1);
		assert(ret == 0);
		if (!(i == 2 || i == 8)) {
			ret = osd_command_attr_build(&cmd, &attr, 1);
			assert(ret == 0);
		}
		ret = osdemu_cmd_submit(osd, cmd.cdb, cmd.outdata, cmd.outlen,
					&data_out, &data_out_len,
					sense_out, &senselen_out);
		assert(ret == 0);
		osd_command_attr_free(&cmd);
	}

	/*
	 * Set some random attributes for querying.
	 */
	uint32_t page = USEROBJECT_PG + LUN_PG_LB;
	set_attr_int(osd, pid, oid,   page, 1, 4);
	set_attr_int(osd, pid, oid+1, page, 1, 49);
	set_attr_int(osd, pid, oid+1, page, 2, 130);
	set_attr_int(osd, pid, oid+2, page, 1, 20);
	set_attr_int(osd, pid, oid+3, page, 1, 101);
	set_attr_int(osd, pid, oid+4, page, 1, 59);
	set_attr_int(osd, pid, oid+4, page, 2, 37);
	set_attr_int(osd, pid, oid+5, page, 1, 75);
	set_attr_int(osd, pid, oid+6, page, 1, 200);
	set_attr_int(osd, pid, oid+7, page, 1, 67);
	set_attr_int(osd, pid, oid+8, page, 1, 323);
	set_attr_int(osd, pid, oid+8, page, 2, 44);
	set_attr_int(osd, pid, oid+9, page, 1, 1);
	set_attr_int(osd, pid, oid+9, page, 2, 19);

	/*
	 * Various queries.
	 */
	/* run without query criteria */
	uint8_t buf[1024], *cp, *matches;
	uint32_t qll;
	uint64_t matchlen;
	uint64_t idlist[8];
	
	qll = MINQLISTLEN;
	memset(buf, 0, 1024);
	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);

	idlist[0] = oid;
	idlist[1] = oid+1;
	idlist[2] = oid+3;
	idlist[3] = oid+4;
	idlist[4] = oid+5;
	idlist[5] = oid+6;
	idlist[6] = oid+7;
	idlist[7] = oid+9;
	check_results(matches, matchlen, idlist, 8);
	free(matches);

	/* run one query without min/max constraints */
	qll = 0;
	memset(buf, 0, 1024);
	cp = buf;
	set_qce(&cp[4], page, 2, 0, NULL, 0, NULL);
	qll += 4 + (4+4+4+2+2);

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);

	idlist[0] = oid+1; 
	idlist[1] = oid+4; 
	idlist[2] = oid+9;
	check_results(matches, matchlen, idlist, 3);

	/* run one query with criteria */
	uint64_t min, max;
	qll = 0;
	min = 40, max= 80;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	memset(buf, 0, 1024);
	cp = buf;
	cp[0] = 0x0;
	set_qce(&cp[4], page, 1, sizeof(min), &min, sizeof(max), &max);
	qll += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);

	idlist[0] = oid+1; 
	idlist[1] = oid+4; 
	idlist[2] = oid+5;
	idlist[3] = oid+7;
	check_results(matches, matchlen, idlist, 4);

	/* run union of two query criteria */
	qll = 0;

	/* first query */
	min = 100, max = 180;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	memset(buf, 0, 1024);
	cp = buf;
	cp[0] = 0x0; /* UNION */
	set_qce(&cp[4], page, 1, sizeof(min), &min, sizeof(max), &max);
	qll += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));
	cp += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));

	/* second query */
	min = 200, max = 323;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	set_qce(cp, page, 1, sizeof(min), &min, sizeof(max), &max);
	qll += (4+4+4+2+sizeof(min)+2+sizeof(max));
	cp += (4+4+4+2+sizeof(min)+2+sizeof(max));

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);

	idlist[0] = oid+3; 
	idlist[1] = oid+6; 
	check_results(matches, matchlen, idlist, 2);

	/* run intersection of 2 query criteria */
	qll = 0;

	/* first query */
	min = 4, max = 100;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	memset(buf, 0, 1024);
	cp = buf;
	cp[0] = 0x1; /* INTERSECTION */
	set_qce(&cp[4], page, 1, sizeof(min), &min, sizeof(max), &max);
	qll += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));
	cp += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));

	/* second query */
	min = 10, max = 400;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	set_qce(cp, page, 2, sizeof(min), &min, sizeof(max), &max);
	qll += (4+4+4+2+sizeof(min)+2+sizeof(max));
	cp += (4+4+4+2+sizeof(min)+2+sizeof(max));

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);

	idlist[0] = oid+1; 
	idlist[1] = oid+4; 
	check_results(matches, matchlen, idlist, 2);

	/* run union of 3 query criteria, with missing min/max */
	qll = 0;

	/* first query */
	min = 130, max = 130;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	memset(buf, 0, 1024);
	cp = buf;
	cp[0] = 0x0; /* UNION */
	set_qce(&cp[4], page, 2, sizeof(min), &min, sizeof(max), &max);
	qll += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));
	cp += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));

	/* second query */
	min = 150;
	set_htonll((uint8_t *)&min, min);
	set_qce(cp, page, 1, sizeof(min), &min, 0, NULL);
	qll += (4+4+4+2+sizeof(min)+2+0);
	cp += (4+4+4+2+sizeof(min)+2+0);

	/* third query */
	max = 10;
	set_htonll((uint8_t *)&max, max);
	set_qce(cp, page, 1, 0, NULL, sizeof(max), &max);
	qll += (4+4+4+2+0+2+sizeof(max));
	cp += (4+4+4+2+0+2+sizeof(max));

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);

	idlist[3] = oid;
	idlist[4] = oid+1;
	idlist[5] = oid+6;
	idlist[2] = oid+9;
	check_results(matches, matchlen, idlist, 4);

	/* set some attributes with text values */
	set_attr_val(osd, pid, oid,   page, 1, "hello", 6);
	set_attr_val(osd, pid, oid+1, page, 1, "cat", 4);
	set_attr_int(osd, pid, oid+1, page, 2, 130);
	set_attr_int(osd, pid, oid+2, page, 1, 20);
	set_attr_val(osd, pid, oid+3, page, 1, "zebra", 6);
	set_attr_int(osd, pid, oid+4, page, 1, 59);
	set_attr_int(osd, pid, oid+4, page, 2, 37);
	set_attr_int(osd, pid, oid+5, page, 1, 75);
	set_attr_val(osd, pid, oid+6, page, 1, "keema", 6);
	set_attr_int(osd, pid, oid+7, page, 1, 67);
	set_attr_int(osd, pid, oid+8, page, 1, 323);
	set_attr_int(osd, pid, oid+8, page, 2, 44);
	set_attr_int(osd, pid, oid+9, page, 1, 1);
	set_attr_val(osd, pid, oid+9, page, 2, "hotelling", 10);

	/* run queries on different datatypes, with diff min max lengths */
	qll = 0;

	/* first query */
	min = 41, max = 169;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	memset(buf, 0, 1024);
	cp = buf;
	cp[0] = 0x0; /* UNION */
	set_qce(&cp[4], page, 1, sizeof(min), &min, sizeof(max), &max);
	qll += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));
	cp += 4 + (4+4+4+2+sizeof(min)+2+sizeof(max));

	/* second query */
	set_qce(cp, page, 1, 3, "ab", 5, "keta");
	qll += (4+4+4+2+2+2+5);
	cp += (4+4+4+2+2+2+5);

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);
	idlist[3] = oid;
	idlist[4] = oid+1;
	idlist[0] = oid+4; 
	idlist[1] = oid+5; 
	idlist[5] = oid+6;
	idlist[2] = oid+7;
	check_results(matches, matchlen, idlist, 6);

	/* run intersection of 3 query criteria, with missing min/max */
	qll = 0;

	/* first query */
	memset(buf, 0, 1024);
	cp = buf;
	cp[0] = 0x1; /* INTERSECTION */
	set_qce(&cp[4], page, 1, 2, "a", 3, "zz");
	qll += 4 + (4+4+4+2+2+2+3);
	cp += 4 + (4+4+4+2+2+2+3);

	/* second query */
	min = 140;
	set_htonll((uint8_t *)&min, min);
	set_qce(cp, page, 1, sizeof(min), &min, 0, NULL);
	qll += (4+4+4+2+sizeof(min)+2+0);
	cp += (4+4+4+2+sizeof(min)+2+0);

	/* third query */
	set_qce(cp, page, 2, 0, NULL, 6, "alpha");
	qll += (4+4+4+2+0+2+6);
	cp += (4+4+4+2+0+2+6);

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);
	idlist[0] = oid+1;
	check_results(matches, matchlen, idlist, 1);

	/* run intersection of 2 query criteria with empty result */
	qll = 0;

	/* first query */
	memset(buf, 0, 1024);
	cp = buf;
	cp[0] = 0x1; /* INTERSECTION */
	set_qce(&cp[4], page, 1, 3, "aa", 4, "zzz");
	qll += 4 + (4+4+4+2+3+2+4);
	cp += 4 + (4+4+4+2+3+2+4);

	/* second query */
	min = 50;
	max = 80;
	set_htonll((uint8_t *)&min, min);
	set_htonll((uint8_t *)&max, max);
	set_qce(cp, page, 1, sizeof(min), &min, sizeof(max), &max);
	qll += (4+4+4+2+sizeof(min)+2+sizeof(max));
	cp += (4+4+4+2+sizeof(min)+2+sizeof(max));

	ret = osd_command_set_query(&cmd, pid, cid, qll, 4096);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, buf, qll, &matches,
				&matchlen, sense_out, &senselen_out);
	assert(ret == 0);
	check_results(matches, matchlen, idlist, 0);

	/*
	 * Cleanup.
	 */
	for (i=0; i<10; i++) {
		ret = osd_command_set_remove(&cmd, pid, oid + i);
		assert(ret == 0);
		ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
					&data_out_len, sense_out,
					&senselen_out);
		assert(ret == 0);
	}
	ret = osd_command_set_remove_collection(&cmd, pid, cid, 0);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	ret = osd_command_set_remove_partition(&cmd, pid);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);
}

struct test_attr {
	uint8_t type;
	uint64_t oid;
	uint32_t page;
	uint32_t number;
	uint64_t intval;
	uint16_t valen;
	const void *val;
}; 

static void ismember_attr(struct test_attr *attr, size_t sz, uint64_t oid,
			  uint32_t page, uint32_t number, uint64_t valen,
			  const void *val)
{
	size_t i = 0;
	for (i = 0; i < sz; i++) {
		if (attr[i].oid == oid && attr[i].page == page &&
		    attr[i].number == number) {
			assert(valen <= attr[i].valen);
			if (attr[i].type == 1) {
				if (valen == attr[i].valen)
					assert(attr[i].intval == ntohll(val));
			} else {
				assert(memcmp(attr[i].val, val, valen) == 0);
			}
			return;
		}
	}
	fprintf(stderr, "unknown attr: oid: %llu, page %u, number %u",
		llu(oid), page, number);
	assert(0); /* unknown attr */
}

void test_list(struct osd_device *osd)
{
	struct osd_command cmd;
	uint64_t pid = PARTITION_PID_LB;
	uint64_t cid = 0;
	uint64_t oid = 0; 
	const void *data_in;
	uint8_t *data_out;
	uint8_t *cp;
	uint32_t page = 0, number = 0;
	uint64_t data_in_len, data_out_len;
	uint64_t idlist[64];
	uint8_t sense_out[OSD_MAX_SENSE];
	uint32_t attr_list_len = 0;
	uint16_t len = 0;
	int senselen_out;
	int i, ret;

	/* create partition */
	ret = osd_command_set_create_partition(&cmd, pid);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* create collection */
	ret = osd_command_set_create_collection(&cmd, pid, cid);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* create 6 objects */
	ret = osd_command_set_create(&cmd, pid, 0, 6);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out, 
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* create another collection */
	ret = osd_command_set_create_collection(&cmd, pid, cid);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* create 4 objects */
	ret = osd_command_set_create(&cmd, pid, 0, 4);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out, 
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);

	/* set attributes on userobjects */
	page = USEROBJECT_PG + LUN_PG_LB;
	number = 1;
	oid = COLLECTION_OID_LB + 1;
	struct test_attr attrs[] = {
		{1, oid, page, number, 1, 8, NULL},
		{1, oid, page+1, number+1, 768, 8, NULL},
		{2, oid, page+2, number+2, 0, 5, "sudo"}, 
		{1, oid+1, page+1, number+1, 56, 8, NULL},
		{1, oid+1, page+2, number+2, 68, 8, NULL},
		{2, oid+2, page+2, number+2, 0, 9, "deadbeef"}, 
		{1, oid+3, page+3, number+3, 1, 8, NULL},
		{1, oid+3, page+1, number+1, 111, 8, NULL},
		{2, oid+3, page+4, number+4, 0, 5, "sudo"}, 
		{1, oid+3, page+2, number+2, 11, 8, NULL},
		{1, oid+3, page+5, number+5, 111111, 8, NULL},
		{2, oid+4, page+4, number+4, 0, 6, "milli"}, 
		{2, oid+4, page+5, number+5, 0, 10, "kilometer"},
		{2, oid+4, page+3, number+3, 0, 11, "hectameter"},
		{2, oid+5, page+1, number+1, 0, 12, "zzzzzzhhhhh"}, 
		{2, oid+5, page+2, number+2, 0, 2, "b"},
		{1, oid+5, page+3, number+3, 6, 8, NULL},
		{1, oid+7, page+1, number+1, 486, 8, NULL}, 
		{1, oid+7, page+4, number+4, 586, 8, NULL},
		{1, oid+7, page+2, number+2, 686, 8, NULL},
		{1, oid+8, page, number, 4, 8, NULL},
		{2, oid+9, page+1, number+1, 0, 14, "setting these"}, 
		{2, oid+9, page+2, number+2, 0, 11, "attributes"},
		{2, oid+9, page+3, number+3, 0, 8, "made me"},
		{2, oid+9, page+4, number+4, 0, 12, "mad! really"}, 
		{1, oid+10, page+1, number+1, 1234567890, 8, NULL},
		{2, oid+10, page, number, 0, 6, "DelTa"},
	};
	for (i = 0; i < ARRAY_SIZE(attrs); i++) {
		if (attrs[i].type == 1) {
			set_attr_int(osd, pid, attrs[i].oid, attrs[i].page,
				     attrs[i].number, attrs[i].intval);
		} else {
			set_attr_val(osd, pid, attrs[i].oid, attrs[i].page,
				     attrs[i].number, attrs[i].val,
				     attrs[i].valen);
		}
	}

	/* set some attributes on collections */
	page = COLLECTION_PG + LUN_PG_LB;
	cid = COLLECTION_OID_LB;
	set_attr_int(osd, pid, cid,   page, 1, 1);
	set_attr_int(osd, pid, cid+7, page, 2, 2);

	/* execute list command. get only oids first */
	ret = osd_command_set_list(&cmd, pid, 0, 4096, 0, 0);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);
	cp = data_out;
	assert(ntohll(cp) == 10*8+16);
	assert(data_out_len == 10*8+24);
	cp += 8;
	assert(ntohll(cp) == 0);
	cp += 8;
	assert(ntohl(cp) == 0);
	cp += 7;
	assert(cp[0] == (0x21 << 2));
	cp += 1;
	data_out_len -= 24;
	oid = COLLECTION_OID_LB + 1;
	for (i = 0; i < 6; i++)
		idlist[i] = oid + i;
	oid = COLLECTION_OID_LB + 1 + i + 1;
	for (i = 0; i < 4; i++)
		idlist[6+i] = oid + i;
	while (data_out_len > 0) {
		assert(ismember(ntohll(cp), idlist, 10));
		cp += 8;
		data_out_len -= 8;
	}
	free(data_out);
	osd_command_attr_free(&cmd);

	/* execute list command with less space */
	ret = osd_command_set_list(&cmd, pid, 0, 72, 0, 0);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cmd.cdb, NULL, 0, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);
	cp = data_out;
	assert(ntohll(cp) == 10*8+16);
	assert(data_out_len == 72);
	cp += 8;
	assert(ntohll(cp) == COLLECTION_OID_LB + 8);
	cp += 8;
	assert(ntohl(cp) == 0);
	cp += 7;
	assert(cp[0] == (0x21 << 2));
	cp += 1;
	data_out_len -= 24;
	oid = COLLECTION_OID_LB + 1;
	for (i = 0; i < 6; i++)
		idlist[i] = oid + i;
	for (i = 0; i < 4; i++)
		idlist[6+i] = 0;
	while (data_out_len > 0) {
		assert(ismember(ntohll(cp), idlist, 10));
		cp += 8;
		data_out_len -= 8;
	}
	free(data_out);
	osd_command_attr_free(&cmd);
				
	/* execute list with attr */
	ret = osd_command_set_list(&cmd, pid, 0, 4096, 0, 1);
	assert(ret == 0);
	page = USEROBJECT_PG + LUN_PG_LB;
	number = 1;
	struct attribute_list getattr[] = {
		{ATTR_GET, page, number, NULL, 0, 0},
		{ATTR_GET, page+1, number+1, NULL, 0, 0},
		{ATTR_GET, page+2, number+2, NULL, 0, 0},
		{ATTR_GET, page+3, number+3, NULL, 0, 0},
		{ATTR_GET, page+4, number+4, NULL, 0, 0},
		{ATTR_GET, page+5, number+5, NULL, 0, 0},
	};
	ret = osd_command_attr_build(&cmd, getattr, 6);
	assert(ret == 0);
	data_in = cmd.outdata;
	data_in_len = cmd.outlen;
	ret = osdemu_cmd_submit(osd, cmd.cdb, data_in, data_in_len, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);
	cp = data_out;
	assert(data_out_len == 792);
	assert(ntohll(cp) == 784);
	cp += 8;
	assert(ntohll(cp) == 0);
	cp += 8;
	assert(ntohl(cp) == 0);
	cp += 7;
	assert(cp[0] == (0x22 << 2));
	cp += 1;
	oid = 0;
	attr_list_len = 0;
	len = 0;
	data_out_len -= 24;
	while (data_out_len > 0) {
		oid = ntohll(cp);
		cp += 12;
		attr_list_len = ntohl(cp);
		cp += 4;
		data_out_len -= 16;
		while (attr_list_len > 0) {
			page = ntohl(cp);
			cp += 4;
			number = ntohl(cp);
			cp += 4;
			len = ntohs(cp);
			cp += 2;
			ismember_attr(attrs, ARRAY_SIZE(attrs), oid, page, 
				      number, len, cp);
			cp += len;
			cp += roundup8(2+len) - (2+len);
			data_out_len -= roundup8(4+4+2+len);
			attr_list_len -= roundup8(4+4+2+len); 
		}
	}
	free(data_out);
	osd_command_attr_free(&cmd);

	/* execute list with attr, alloc length less than required */
	ret = osd_command_set_list(&cmd, pid, 0, 200, 0, 1);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, getattr, 6);
	assert(ret == 0);
	data_in = cmd.outdata;
	data_in_len = cmd.outlen;
	ret = osdemu_cmd_submit(osd, cmd.cdb, data_in, data_in_len, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);
	cp = data_out;
	assert(data_out_len == data_out_len);
	assert(data_out_len == 200);
	assert(ntohll(cp) == 784);
	cp += 8;
	assert(ntohll(cp) == 65539);
	cp += 8;
	assert(ntohl(cp) == 0);
	cp += 7;
	assert(cp[0] == (0x22 << 2));
	cp += 1;
	oid = 0;
	attr_list_len = 0;
	len = 0;
	data_out_len -= 24;
	while (data_out_len > 0) {
		oid = ntohll(cp);
		cp += 12;
		attr_list_len = ntohl(cp);
		cp += 4;
		data_out_len -= 16;
		while (attr_list_len > 0) {
			page = ntohl(cp);
			cp += 4;
			number = ntohl(cp);
			cp += 4;
			len = ntohs(cp);
			cp += 2;
			ismember_attr(attrs, ARRAY_SIZE(attrs), oid, page, 
				      number, len, cp);
			cp += len;
			cp += roundup8(2+len) - (2+len);
			data_out_len -= roundup8(4+4+2+len);
			attr_list_len -= roundup8(4+4+2+len); 
		}
	}
	free(data_out);
	osd_command_attr_free(&cmd);

#if 0
	/* execute list with attr, alloc length less than required */
	ret = osd_command_set_list(&cmd, pid, 0, 208, 0, 1);
	assert(ret == 0);
	ret = osd_command_attr_build(&cmd, getattr, 6);
	assert(ret == 0);
	data_in = cmd.outdata;
	data_in_len = cmd.outlen;
	ret = osdemu_cmd_submit(osd, cmd.cdb, data_in, data_in_len, &data_out,
				&data_out_len, sense_out, &senselen_out);
	assert(ret == 0);
	cp = data_out;
	assert(data_out_len == data_out_len);
	assert(ntohll(cp) == 792);
	cp += 8;
	assert(ntohll(cp) == 0);
	cp += 8;
	assert(ntohl(cp) == 0);
	cp += 7;
	assert(cp[0] == (0x22 << 2));
	cp += 1;
	oid = 0;
	attr_list_len = 0;
	len = 0;
	data_out_len -= 24;
	while (data_out_len > 0) {
		oid = ntohll(cp);
		cp += 12;
		attr_list_len = ntohl(cp);
		cp += 4;
		data_out_len -= 16;
		while (attr_list_len > 0) {
			page = ntohl(cp);
			cp += 4;
			number = ntohl(cp);
			cp += 4;
			len = ntohs(cp);
			cp += 2;
			ismember_attr(attrs, ARRAY_SIZE(attrs), oid, page, 
				      number, len, cp);
			cp += len;
			cp += roundup8(2+len) - (2+len);
			data_out_len -= roundup8(4+4+2+len);
			attr_list_len -= roundup8(4+4+2+len); 
		}
	}
	free(data_out);
	osd_command_attr_free(&cmd);

	struct test_attr complexattrs[] = {
		{1, oid, page, number, 1, 0, NULL},
		{1, oid, page+1, number+3, 768, 0, NULL},
		{2, oid, page+12, number+20, 0, 5, "sudo"}, 
		{1, oid+1, page+40, number, 56, 0, NULL},
		{1, oid+1, page+1, number+30, 68, 0, NULL},
		{2, oid+2, page+2, number+21, 0, 9, "deadbeef"}, 
		{1, oid+3, page+3, number+3, 1, 0, NULL},
		{1, oid+3, page+5, number+1, 111, 0, NULL},
		{1, oid+3, page+7, number, 0, 1111, "sudo"}, 
		{1, oid+3, page+2, number+45, 11, 0, NULL},
		{1, oid+3, page+11, number+6, 111111, 0, NULL},
		{2, oid+4, page+4, number+11, 0, 6, "milli"}, 
		{2, oid+4, page+5, number+13, 0, 10, "kilometer"},
		{2, oid+4, page+6, number+17, 0, 11, "hectameter"},
		{2, oid+5, page+1, number+3, 0, 12, "zzzzzzhhhhh"}, 
		{2, oid+5, page+2, number+2, 0, 2, "b"},
		{1, oid+5, page+3, number+1, 6, 0, NULL},
		{1, oid+7, page+38, number+2, 486, 0, NULL}, 
		{1, oid+7, page+39, number+4, 586, 0, NULL},
		{1, oid+7, page+40, number+8, 686, 0, NULL},
		{1, oid+8, page, number, 4, 0, NULL},
		{2, oid+9, page+12, number+2000, 0, 14, "setting these"}, 
		{2, oid+9, page+32, number+4000, 0, 11, "attributes"},
		{2, oid+9, page+43, number+6001, 0, 8, "made me"},
		{2, oid+9, page+54, number+7293, 0, 12, "mad! really"}, 
		{1, oid+10, page+1, number+3, 1234567890, 0, NULL},
		{2, oid+10, page, number+40, 0, 6, "DelTa"},
	};
#endif 
}


int main()
{
	int ret = 0;
	const char *root = "/tmp/osd/";
	struct osd_device osd;

	system("rm -rf /tmp/osd");
	ret = osd_open(root, &osd);
	assert(ret == 0);

	/* test_partition(&osd); */
	/* test_create(&osd); */
	/* test_query(&osd); */
	test_list(&osd);

	ret = osd_close(&osd);
	assert(ret == 0);

	return 0;
}
