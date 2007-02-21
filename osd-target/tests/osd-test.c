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

void test_osd_create(struct osd_device *osd);
void test_osd_set_attributes(struct osd_device *osd);
void test_osd_format(struct osd_device *osd);
void test_osd_read_write(struct osd_device *osd);
void test_osd_create_partition(struct osd_device *osd);
void test_osd_get_attributes(struct osd_device *osd);

void test_osd_create(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_create(osd, 0, 1, 0, sense);
	if (ret != 0)
		osd_error("0, 1, 0 failed as expected");
	ret = osd_create(osd, USEROBJECT_PID_LB, 1, 0, sense);
	if (ret != 0)
		osd_error("USEROBJECT_PID_LB, 1, 0 failed as expected");
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 2, sense);
	if (ret != 0)
		osd_error("USEROBJECT_PID_LB, USEROBJECT_OID_LB, 2 failed as "
		      "expected");
	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("osd_create_partition PARTITION_PID_LB failed");
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_PID_LB, 0, sense);
	if (ret != 0)
		osd_error_fatal("USEROBJECT_PID_LB, USEROBJECT_PID_LB, 0 failed");
	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("USEROBJECT_PID_LB, USEROBJECT_PID_LB");
	/* remove non-existing object */
	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_PID_LB, sense);
	if (ret == 0)
		osd_debug("removal of non-existing object succeeded");
	else
		osd_debug("removal of non-existing object failed");
	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("osd_remove_partition PARTITION_PID_LB failed");

	free(sense);
}

void test_osd_set_attributes(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);
	void *val = Calloc(1, 1024);

	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("osd_create_partition PARTITION_PID_LB failed");
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	if (ret != 0)
		osd_error_fatal("USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0 failed");

	ret = osd_set_attributes(osd, ROOT_PID, ROOT_OID, 0, 0, 
				 NULL, 0, EMBEDDED, sense);
	if (ret != 0)
		osd_error("osd_set_attributes root failed as expected");
	ret = osd_set_attributes(osd, PARTITION_PG_LB, PARTITION_OID, 0, 0, 
				 NULL, 0, EMBEDDED, sense);
	if (ret != 0)
		osd_error("osd_set_attributes partition failed as expected");
	ret = osd_set_attributes(osd, COLLECTION_PID_LB, COLLECTION_OID_LB, 0, 
				 0, NULL, 0, EMBEDDED, sense);
	if (ret != 0)
		osd_error("osd_set_attributes collection failed as expected");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0,
				 0, NULL, 0, EMBEDDED, sense);
	if (ret != 0)
		osd_error("osd_set_attributes userobject failed as expected");

	sprintf(val, "This is test, long test more than forty bytes");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG_LB, 0, val, strlen(val)+1, 
				 EMBEDDED, sense);
	if (ret != 0)
		osd_error("osd_set_attributes number failed as expected");

	sprintf(val, "Madhuri Dixit");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG_LB, 1, val, strlen(val)+1, 
				 EMBEDDED, sense);
	if (ret != 0)
		osd_error_fatal("osd_set_attributes failed for mad dix");

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	if (ret != 0)
		osd_error_fatal("osd_remove USEROBJECT_PID_LB, USEROBJECT_OID_LB");

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("osd_remove_partition PARTITION_PID_LB failed");

	free(sense);
	free(val);
}

void test_osd_format(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_format_osd(osd, 0, sense);
	if (ret != 0)
		osd_error_errno("%s:osd_format", __func__);

	free(sense);
}

void test_osd_read_write(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);
	void *mybuf = Calloc(1, 256);
	uint8_t *buf = Calloc(1, 256);
	uint64_t len;

	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("osd_create_partition PARTITION_PID_LB failed");
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	if (ret != 0)
		osd_error_errno("USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0 failed");

	sprintf(mybuf, "Hello World! Get life\n");
	ret = osd_write(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
			strlen(mybuf)+1, 0, mybuf, sense);
	if (ret != 0)
		osd_error_errno("osd_write failed ret %d", ret);

	ret = osd_read(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
		       256, 0, buf, &len, sense);
	if (ret != 0)
		osd_error_errno("osd_read failed ret %d", ret);
	osd_debug("osd_read len %llu buf: %s", llu(len), buf);
	if (strcmp((void *) buf, mybuf))
		osd_error_fatal("buf and mybuf differ!");
	if (buf)
		free(buf);

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	if (ret != 0)
		osd_error_errno("osd_remove USEROBJECT_PID_LB, USEROBJECT_OID_LB");

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("osd_remove_partition PARTITION_PID_LB failed");
	free(sense);
	free(mybuf);
}

void test_osd_create_partition(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_create_partition(osd, 0, sense);
	if (ret != 0)
		osd_error_fatal("osd_create_partition failed");
	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error("PARTITION_PID_LB osd_create_partition failed as exp.");
	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_errno("osd_remove_partition PARTITION_PID_LB");
	/* remove non-existing object */
	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	if (ret == 0)
		osd_debug("removal of non-existing partition succeeded");
	else
		osd_debug("removal of non-existing partition failed");

	free(sense);
}

void test_osd_get_attributes(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);
	void *val = Calloc(1, 1024);
	list_entry_t *le = NULL;
	uint8_t *cp = NULL;
	uint16_t len;

	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_fatal("PARTITION_PID_LB, PARTITION_OID failed");

	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	if (ret != 0)
		osd_error_fatal("USEROBJECT_PID_LB, USEROBJECT_OID_LB failed");

	sprintf(val, "Madhuri Dixit");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG_LB, 1, val, strlen(val)+1, 
				 EMBEDDED, sense);
	if (ret != 0)
		osd_error_errno("osd_set_attributes failed for mad dix");

	le = val;
	len = 1024;
	ret = osd_get_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB,
				 USEROBJECT_PG_LB, 1, le, &len, 0, EMBEDDED, 
				 sense);
	if (ret != 0)
		osd_error_fatal("osd_get_attributes failed");
	printf("page %x, number %x, len %d, val %s\n", 
	       ntohl_le((uint8_t *)&le->page), ntohl_le((uint8_t *)&le->number),
	       ntohs_le((uint8_t *)&le->len), (uint8_t *)le + ATTR_VAL_OFFSET);

	len = 1024;
	ret = osd_get_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB,
				 CUR_CMD_ATTR_PG, 0, val, &len, 1, 
				 EMBEDDED, sense);
	if (ret != 0)
		osd_error_fatal("osd_get_attributes ccap failed");

	cp = val;
	printf("%x %x %d %s\n", ntohl_le((uint8_t *)&le->page), 
	       ntohl_le((uint8_t *)&le->number), ntohs_le((uint8_t *)&le->len),
	       (cp + ATTR_VAL_OFFSET));
	cp += (CCAP_ID_LEN + ATTR_VAL_OFFSET);

	printf("%x %x %d %s\n", ntohl_le((uint8_t *)&le->page), 
	       ntohl_le((uint8_t *)&le->number), ntohs_le((uint8_t *)&le->len),
	       (cp + ATTR_VAL_OFFSET));
	cp += (sizeof(osd->ccap.ricv) + ATTR_VAL_OFFSET);

	printf("%x %x %d %x\n", ntohl_le((uint8_t *)&le->page), 
	       ntohl_le((uint8_t *)&le->number), ntohs_le((uint8_t *)&le->len),
	       *(cp + ATTR_VAL_OFFSET));
	cp += (sizeof(osd->ccap.obj_type) + ATTR_VAL_OFFSET);

	printf("%x %x %d %llx\n", ntohl_le((uint8_t *)&le->page), 
	       ntohl_le((uint8_t *)&le->number), ntohs_le((uint8_t *)&le->len),
	       llu(*(uint64_t *)(cp + ATTR_VAL_OFFSET)));
	cp += (sizeof(osd->ccap.pid) + ATTR_VAL_OFFSET);

	printf("%x %x %d %llx\n", ntohl_le((uint8_t *)&le->page), 
	       ntohl_le((uint8_t *)&le->number), ntohs_le((uint8_t *)&le->len),
	       llu(*(uint64_t *)(cp + ATTR_VAL_OFFSET)));
	cp += (sizeof(osd->ccap.oid) + ATTR_VAL_OFFSET);

	printf("%x %x %d %llx\n", ntohl_le((uint8_t *)&le->page), 
	       ntohl_le((uint8_t *)&le->number), ntohs_le((uint8_t *)&le->len),
	       llu(*(uint64_t *)(cp + ATTR_VAL_OFFSET)));

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	if (ret != 0)
		osd_error_errno("osd_remove USEROBJECT_PID_LB, USEROBJECT_OID_LB");

	ret = osd_remove_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		osd_error_errno("osd_remove_partition PARTITION_PID_LB");

	free(sense);
	free(val);
}

int main()
{
	int ret = 0;
	const char *root = "/tmp/osd/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	if (ret != 0)
		osd_error_errno("%s: osd_open", __func__);

	/*test_osd_create(&osd);*/
	/*test_osd_set_attributes(&osd);*/
	/*test_osd_format(&osd);*/
	/*test_osd_read_write(&osd);*/
	/*test_osd_create_partition(&osd);*/
	test_osd_get_attributes(&osd);

	ret = osd_close(&osd);
	if (ret != 0)
		osd_error_errno("%s: osd_close", __func__);


	return 0;
}

