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

#define CDB_SZ (200)

void test_partition(struct osd_device *osd);

void test_partition(struct osd_device *osd) 
{
	int ret = 0;
	uint8_t cdb[CDB_SZ];
	uint8_t senselen_out;
	uint8_t *data_out, *sense_out;
	uint64_t data_out_len;

	/* create partition */
	ret = set_cdb_osd_create_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				&sense_out, &senselen_out);
	assert(ret == 0);

	/* remove partition */
	ret = set_cdb_osd_remove_partition(cdb, PARTITION_PID_LB);
	assert(ret == 0);
	ret = osdemu_cmd_submit(osd, cdb, NULL, 0, &data_out, &data_out_len,
				&sense_out, &senselen_out);
	assert(ret == 0);

	/* create partition + get attr */
}

int main()
{
	int ret = 0;
	const char *root = "/tmp/osd/";
	struct osd_device osd;

	ret = osd_open(root, &osd);
	assert(ret == 0);

	test_partition(&osd);

	ret = osd_close(&osd);
	assert(ret == 0);

	return 0;
}
