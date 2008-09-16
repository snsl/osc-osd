/*
 * Test atomics, with timing.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

static void fa_test(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	uint64_t outbuf[1], inbuf[1];
	uint64_t oid = USEROBJECT_OID_LB;
	/* const int iter = 10000; */
	const int iter = 1;

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command, pid, oid, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	/* len must be uint64_t, offset is where the results go */
	memset(&command, 0, sizeof(command));
	osd_command_set_fa(&command, pid, oid, 8, 0);
	set_htonll((uint8_t *)outbuf, 1);
	command.outdata = outbuf;
	command.outlen = sizeof(outbuf);
	command.indata = inbuf;
	command.inlen_alloc = sizeof(inbuf);

	/* warm up */
	for (i=0; i<10; i++) {
		inbuf[0] = 0xf000000000000000;
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
		assert(get_ntohll((uint8_t *)inbuf) == (uint64_t) (i));
	}

	for (i=0; i<iter; i++) {
		inbuf[0] = 0xf000000000000000;
		rdtsc(start);
		ret = osd_submit_and_wait(fd, &command);
		rdtsc(end);
		assert(ret == 0);
		assert(get_ntohll((uint8_t *)inbuf) == (uint64_t) (i+10));
		delta = end - start;
		v[i] = (double) delta / mhz;  /* time in usec */
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("fa      %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);

	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);
}

static void cas_test(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	uint64_t outbuf[2], inbuf[1];
	uint64_t oid = USEROBJECT_OID_LB;
	/* const int iter = 10000; */
	const int iter = 1;

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command, pid, oid, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	/* len must be uint64_t, offset is where the results go */
	memset(&command, 0, sizeof(command));
	osd_command_set_cas(&command, pid, oid, 8, 0);
	command.outdata = outbuf;
	command.outlen = sizeof(outbuf);
	command.indata = inbuf;
	command.inlen_alloc = sizeof(inbuf);

	/* warm up */
	for (i=0; i<10; i++) {
		inbuf[0] = 0xffff;
		set_htonll((uint8_t *)&outbuf[0], (i % 2));
		set_htonll((uint8_t *)&outbuf[1], (1 - (i % 2)));
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
		assert(get_ntohll((uint8_t *)inbuf) == 
		       get_ntohll((uint8_t *)outbuf));
	}

	for (i=0; i<iter; i++) {
		inbuf[0] = 0xffff;
		set_htonll((uint8_t *)&outbuf[0], (i % 2));
		set_htonll((uint8_t *)&outbuf[1], (1 - (i % 2)));
		rdtsc(start);
		ret = osd_submit_and_wait(fd, &command);
		rdtsc(end);
		assert(ret == 0);
		assert(get_ntohll((uint8_t *)&inbuf[0]) == 
		       get_ntohll((uint8_t *)&outbuf[0]));
		delta = end - start;
		v[i] = (double) delta / mhz;  /* time in usec */
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("cas     %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);

	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);
}

static void cas_plus_getattr_test(int fd, uint64_t pid)
{
	int i, ret;
	uint64_t start, end, delta;
	double mu, stdev;
	double *v;
	struct osd_command command;
	uint64_t outbuf[2], inbuf[1];
	uint64_t oid = USEROBJECT_OID_LB;
	/* const int iter = 10000; */
	const int iter = 1;
	const struct attribute_list attrs[] = {
	    { .type = ATTR_GET, .page = USER_TMSTMP_PG, .number = UTSAP_CTIME,
	      .len = 6 },
	    { .type = ATTR_GET, .page = USER_INFO_PG, .number = UIAP_OID,
	      .len = 8 },
	};

	v = malloc(iter * sizeof(*v));
	if (!v)
		osd_error_fatal("out of memory");

	osd_command_set_create(&command, pid, oid, 1);
	ret = osd_submit_and_wait(fd, &command);
	assert(ret == 0);

	/* len must be uint64_t, offset is where the results go */
	memset(&command, 0, sizeof(command));
	osd_command_set_cas(&command, pid, oid, 8, 0);
	command.outdata = outbuf;
	command.outlen = sizeof(outbuf);
	command.indata = inbuf;
	command.inlen_alloc = sizeof(inbuf);

	osd_command_attr_build(&command, attrs, 2);

	/* warm up */
	for (i=0; i<10; i++) {
		inbuf[0] = 0xffff;
		set_htonll((uint8_t *)&outbuf[0], (i % 2));
		set_htonll((uint8_t *)&outbuf[1], (1 - (i % 2)));
		ret = osd_submit_and_wait(fd, &command);
		assert(ret == 0);
		assert(get_ntohll((uint8_t *)inbuf) == 
		       get_ntohll((uint8_t *)outbuf));
	}

	for (i=0; i<iter; i++) {
		inbuf[0] = 0xffff;
		set_htonll((uint8_t *)&outbuf[0], (i % 2));
		set_htonll((uint8_t *)&outbuf[1], (1 - (i % 2)));
		rdtsc(start);
		ret = osd_submit_and_wait(fd, &command);
		rdtsc(end);
		assert(ret == 0);
		assert(get_ntohll((uint8_t *)&inbuf[0]) == 
		       get_ntohll((uint8_t *)&outbuf[0]));
		delta = end - start;
		v[i] = (double) delta / mhz;  /* time in usec */
		osd_command_attr_resolve(&command);
		assert(command.attr[0].outlen == 6);
		assert(command.attr[1].outlen == 8);
		/* ignore ts */
		/* uint64_t ts = get_ntohtime(command.attr[0].val); */
		assert(get_ntohll(command.attr[1].val) == oid);
	}

	mu = mean(v, iter);
	stdev = stddev(v, mu, iter);
	printf("cas+ga  %9.3lf +- %8.3lf\n", mu, stdev);
	free(v);

	osd_command_attr_free(&command);

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

	cas_test(fd, PARTITION_PID_LB);
	fa_test(fd, PARTITION_PID_LB);
	cas_plus_getattr_test(fd, PARTITION_PID_LB);

	close(fd);
	return 0;
}

