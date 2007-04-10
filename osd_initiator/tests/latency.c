/*
 * Per-operation timings for a few critical operations.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "util/util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

static void noop_test(int fd)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	const int iter = 10000;

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_test_unit_ready(&command);

	/* warm up */
	for (i=0; i<50; i++) {
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
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("# noop    %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);
}

static void getattr_test(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	uint64_t oid = USEROBJECT_OID_LB;
	struct attribute_list attr = {
		.type = ATTR_GET,
		.page = USER_INFO_PG,
		.number = UIAP_LOGICAL_LEN,
		.len = 8,
	};
	const int iter = 10000;

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command, pid, oid, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	osd_command_set_get_attributes(&command, pid, oid);
	osd_command_attr_build(&command, &attr, 1);

	/* warm up */
	for (i=0; i<5; i++) {
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
	}

	/* free memory */
	osd_command_attr_resolve(&command);

	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("# getattr %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);
}

static void create_test(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	uint64_t oid = USEROBJECT_OID_LB;
	const int iter = 10000;
	struct attribute_list attr = { ATTR_GET, CUR_CMD_ATTR_PG,
				       CCAP_OID, NULL, CCAP_OID_LEN, 0};

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command, pid, oid, 1);
        osd_command_attr_build(&command, &attr, 1);

	/* warm up */
	for (i=0; i<50; i++) {
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
		++oid;
		set_htonll(&command.cdb[24], oid);
	}

	set_htonll(&command.cdb[24], 0);  /* auto-gen oid */
	for (i=0; i<iter; i++) {
		rdtsc(start);
		ret = osd_submit_and_wait(fd, &command);
		rdtsc(end);
		assert(ret == 0);
		delta = end - start;
		v[i] = (double) delta / mhz;  /* time in usec */
#if 0
		/* much faster to select an oid than to use autogen */
		++oid;
		set_htonll(&command.cdb[24], oid);
#endif
	}

	while (--oid >= USEROBJECT_OID_LB) {
		osd_command_set_remove(&command, pid, oid);
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	/*
	 * XXX: Be sure to pay attention to the values, they creep up
	 * due to database overheads as the number of objects grows.
	 */
	printf("# create  %9.3lf +- %8.3lf\n", mu, stdev);
	if (0) {
		for (i=0; i<iter; i++)
			printf("%9.3lf\n", v[i]);
	}
	free(v);
}

static void remove_test(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	uint64_t oid = USEROBJECT_OID_LB;
	const int iter = 10000;
	const int warmup = 50;

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	for (i=0; i<iter+warmup; i++) {
		osd_command_set_create(&command, pid, oid, 1);
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
		++oid;
	}

	oid = USEROBJECT_OID_LB;
	osd_command_set_remove(&command, pid, oid);

	/* warm up */
	for (i=0; i<warmup; i++) {
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
		++oid;
		set_htonll(&command.cdb[24], oid);
	}

	for (i=0; i<iter; i++) {
		rdtsc(start);
		ret = osd_submit_and_wait(fd, &command);
		rdtsc(end);
		assert(ret == 0);
		delta = end - start;
		v[i] = (double) delta / mhz;  /* time in usec */
		++oid;
		set_htonll(&command.cdb[24], oid);
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("# remove  %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);
}

static void create_remove_test(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command_create, command_remove;
	uint64_t oid = USEROBJECT_OID_LB;
	const int iter = 10000;

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command_create, pid, oid, 1);
	osd_command_set_remove(&command_remove, pid, oid);

	/* warm up */
	for (i=0; i<50; i++) {
		ret = osd_submit_and_wait(fd, &command_create);
		ret += osd_submit_and_wait(fd, &command_remove);
		assert(ret == 0);
	}

	for (i=0; i<iter; i++) {
		rdtsc(start);
		ret = osd_submit_and_wait(fd, &command_create);
		ret += osd_submit_and_wait(fd, &command_remove);
		rdtsc(end);
		assert(ret == 0);
		delta = end - start;
		v[i] = (double) delta / mhz;  /* time in usec */
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("# cr+rm   %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);
}

static void get_set_attr(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	char attr_val[] = "deadbEEf";
	char ret_val[16];
	struct osd_command command;
	uint64_t oid = USEROBJECT_OID_LB;
	struct attribute_list attr = {
		.type = ATTR_SET,
		.page = USEROBJECT_PG + LUN_PG_LB,
		.number = 1,
		.val = attr_val,
		.len = sizeof(attr_val),
	};
	const int iter = 1000;
	const int warm_iter = 5;

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command, pid, oid, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	osd_command_set_set_attributes(&command, pid, oid);
	osd_command_attr_build(&command, &attr, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	attr.type = ATTR_GET;
	attr.val = ret_val;
	memset(ret_val, 0, sizeof(ret_val));
	osd_command_set_get_attributes(&command, pid, oid);
	osd_command_attr_build(&command, &attr, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);
	osd_command_attr_resolve(&command);
	assert(memcmp(attr_val, command.attr[0].val, sizeof(attr_val)) == 0);
	/* free memory */
	osd_command_attr_free(&command);


	attr.type = ATTR_GET;
	attr.val = ret_val;
	memset(ret_val, 0, sizeof(ret_val));
	osd_command_set_get_attributes(&command, pid, oid);
	osd_command_attr_build(&command, &attr, 1);
	/* warm up */
	for (i=0; i<warm_iter; i++) {
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
	}


	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);
	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("# getattr %9.3lf +- %8.3lf\n", mu, stdev);
	for (i=0; i<iter; i++) 
		printf("latency %lf\n", v[i]);
	free(v);
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

	noop_test(fd);
	getattr_test(fd, PARTITION_PID_LB);
	create_test(fd, PARTITION_PID_LB);
	remove_test(fd, PARTITION_PID_LB);
	create_remove_test(fd, PARTITION_PID_LB);
	get_set_attr(fd, PARTITION_PID_LB);

	close(fd);
	return 0;
}

