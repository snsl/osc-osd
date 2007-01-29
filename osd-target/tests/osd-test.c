#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "attr.h"
#include "obj.h"
#include "util.h"

void test_osd_create(struct osd_device *osd);

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
		error_fatal("USEROBJECT_PID_LB, USEROBJECT_PID_LB, 0 failed");
	ret = osd_remove(osd, USEROBJECT_PID_LB, USEROBJECT_PID_LB, sense);
	if (ret != 0)
		error_fatal("USEROBJECT_PID_LB, USEROBJECT_PID_LB");
}

int main()
{
	int ret = 0;
	const char *root = "/tmp/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	if (ret != 0)
		error_fatal("%s: osd_open", __func__);

	test_osd_create(&osd);

	ret = osd_close(&osd);
	if (ret != 0)
		error_fatal("%s: osd_close", __func__);

	return 0;
}

