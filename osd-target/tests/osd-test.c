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

void test_osd_create(struct osd_device *osd);
void test_osd_set_attributes(struct osd_device *osd);
void test_osd_format(struct osd_device *osd);
void test_osd_read_write(struct osd_device *osd);
void test_osd_create_partition(struct osd_device *osd);

void test_osd_create(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_create(osd, 0, 1, 0, sense);
	if (ret != 0)
		error("0, 1, 0 failed as expected");
	ret = osd_create(osd, USEROBJECT_PID_LB, 1, 0, sense);
	if (ret != 0)
		error("USEROBJECT_PID_LB, 1, 0 failed as expected");
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 2, sense);
	if (ret != 0)
		error("USEROBJECT_PID_LB, USEROBJECT_OID_LB, 2 failed as "
		      "expected");
	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_PID_LB, 0, sense);
	if (ret != 0)
		error_errno("USEROBJECT_PID_LB, USEROBJECT_PID_LB, 0 failed");
	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_PID_LB, sense);
	if (ret != 0)
		error_errno("USEROBJECT_PID_LB, USEROBJECT_PID_LB");

	free(sense);
}

void test_osd_set_attributes(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);
	void *val = Calloc(1, 1024);

	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	if (ret != 0)
		error_errno("USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0 failed");

	ret = osd_set_attributes(osd, ROOT_PID, ROOT_OID, 0, 0, 
				 NULL, 0, sense);
	if (ret != 0)
		error("osd_set_attributes root failed as expected");
	ret = osd_set_attributes(osd, PARTITION_PG_LB, PARTITION_OID, 0, 0, 
				 NULL, 0, sense);
	if (ret != 0)
		error("osd_set_attributes partition failed as expected");
	ret = osd_set_attributes(osd, COLLECTION_PID_LB, COLLECTION_OID_LB, 0, 
				 0, NULL, 0, sense);
	if (ret != 0)
		error("osd_set_attributes collection failed as expected");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0,
				 0, NULL, 0, sense);
	if (ret != 0)
		error("osd_set_attributes userobject failed as expected");

	sprintf(val, "This is test, long test more than forty bytes");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG_LB, 0, val, strlen(val)+1, 
				 sense);
	if (ret != 0)
		error("osd_set_attributes number failed as expected");

	sprintf(val, "Madhuri Dixit");
	ret = osd_set_attributes(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
				 USEROBJECT_PG_LB, 1, val, strlen(val)+1, 
				 sense);
	if (ret != 0)
		error_errno("osd_set_attributes failed for mad dix");

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	if (ret != 0)
		error_errno("osd_remove USEROBJECT_PID_LB, USEROBJECT_OID_LB");

	free(sense);
	free(val);
}

void test_osd_format(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_format_osd(osd, 0, sense);
	if (ret != 0)
		error_errno("%s:osd_format", __func__);

	free(sense);
}

void test_osd_read_write(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);
	void *mybuf = Calloc(1, 256);
	void *buf = NULL;
	uint64_t len = 0;

	ret = osd_create(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0, sense);
	if (ret != 0)
		error_errno("USEROBJECT_PID_LB, USEROBJECT_OID_LB, 0 failed");

	sprintf(mybuf, "Hello World! Get life\n");
	ret = osd_write(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
			strlen(mybuf)+1, 0, mybuf, sense);
	if (ret != 0)
		error_errno("osd_write failed ret %d", ret);

	ret = osd_read(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, 
		       strlen(mybuf)+1, 0, (uint8_t **)&buf, &len, sense);
	if (ret != 0)
		error_errno("osd_read failed ret %d", ret);
	debug("osd_read len %llu buf: %s", llu(len), (char *)buf);
	if (strcmp(buf, mybuf))
		error_fatal("buf and mybuf differ!");
	if (buf)
		free(buf);

	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_OID_LB, sense);
	if (ret != 0)
		error_errno("osd_remove USEROBJECT_PID_LB, USEROBJECT_OID_LB");

	free(sense);
	free(mybuf);
}

void test_osd_create_partition(struct osd_device *osd)
{
	int ret = 0;
	void *sense = Calloc(1, 1024);

	ret = osd_create_partition(osd, 0, sense);
	if (ret != 0)
		error_fatal("osd_create_partition failed");
	ret = osd_create_partition(osd, PARTITION_PID_LB, sense);
	if (ret != 0)
		error("PARTITION_PID_LB osd_create_partition failed as exp.");
	free(sense);
}

int main()
{
	int ret = 0;
	const char *root = "/tmp/osd/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	if (ret != 0)
		error_errno("%s: osd_open", __func__);

	/*test_osd_create(&osd);*/
	/*test_osd_set_attributes(&osd);*/
	/*test_osd_format(&osd);*/
	/*test_osd_read_write(&osd);*/
	test_osd_create_partition(&osd);

	ret = osd_close(&osd);
	if (ret != 0)
		error_errno("%s: osd_close", __func__);


	return 0;
}

