/*
 * Test getting all attrs in a page.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

static void attr_test(int fd, uint64_t pid)
{
	int i, j, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	uint64_t oid = USEROBJECT_OID_LB;
	/* const int iter = 10000; */
	const int iter = 1;
	const uint32_t page = LUN_PG_LB + 27;
	char bufs[3][20];
	const struct attribute_list attrs[] = {
	    { .type = ATTR_SET, .page = page, .number = 3, .val = bufs[0],
	      .len = 2 },
	    { .type = ATTR_SET, .page = page, .number = 5, .val = bufs[1],
	      .len = 4 },
	    { .type = ATTR_SET, .page = page, .number = 8, .val = bufs[2],
	      .len = 6 },
	};
	const int numattrs = sizeof(attrs)/sizeof(attrs[0]);

	strcpy(bufs[0], "a");
	strcpy(bufs[1], "bcd");
	strcpy(bufs[2], "efghi");

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command, pid, oid, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	/* toss in some attrs */
	osd_command_set_set_attributes(&command, pid, oid);
	osd_command_attr_build(&command, attrs, numattrs);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);
	osd_command_attr_free(&command);

	/* the get command */
	osd_command_set_get_attributes(&command, pid, oid);
	osd_command_attr_all_build(&command, page);

	/* warm up */
	for (i=0; i<10; i++) {
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
	}

	for (i=0; i<iter; i++) {
		rdtsc(start);
		ret = osd_submit_and_wait(fd, &command);
		rdtsc(end);
		assert(ret == 0);
		delta = end - start;
		v[i] = (double) delta / mhz;  /* time in usec */

		osd_command_attr_all_resolve(&command);
		assert(command.numattr == numattrs);
		for (j=0; j<numattrs; j++) {
			assert(command.attr[j].page = page);
			assert(command.attr[j].number == attrs[j].number);
			assert(command.attr[j].outlen == attrs[j].len);
			assert(strcmp(command.attr[j].val, attrs[j].val) == 0);
		}
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("attrdir %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);

	osd_command_attr_all_free(&command);

	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;

	osd_set_progname(argc, argv); 

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		osd_error("%s: get drive error", __func__);
		return 1;
	}
	if (num_drives == 0) {
		osd_error("%s: no drives", __func__);
		return 1;
	}

	i = 0;
	osd_debug("drive %s name %s", drives[i].chardev, drives[i].targetname);
	fd = open(drives[i].chardev, O_RDWR);
	if (fd < 0) {
		osd_error_errno("%s: open %s", __func__, drives[i].chardev);
		return 1;
	}
	osd_free_drive_list(drives, num_drives);

	format_osd(fd, 1<<30); 
	create_partition(fd, PARTITION_PID_LB);

	attr_test(fd, PARTITION_PID_LB);

	close(fd);
	return 0;
}

