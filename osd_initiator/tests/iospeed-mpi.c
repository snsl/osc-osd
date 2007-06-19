/*
 * MPI version of now-classic iospeed benchmark.  Start like:
 *
 * mpiexec -pernode -n 2 iospeed-mpi
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>

#include <mpi.h>

#include "util/util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

static int rank, numproc;

static uint64_t obj_create_any(int fd, uint64_t pid)
{
	struct osd_command command;
	struct attribute_list attr = {
		.type = ATTR_GET,
		.page = CUR_CMD_ATTR_PG,
		.number = CCAP_OID,
		.len = 8,
	};
	int ret;
	uint64_t oid;

	osd_command_set_create(&command, pid, 0, 0);
	osd_command_attr_build(&command, &attr, 1);
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit_and_wait failed", __func__);
		exit(1);
	}
	ret = osd_command_attr_resolve(&command);
	if (ret) {
		osd_error_xerrno(ret, "%s: attr_resolve failed", __func__);
		exit(1);
	}
	oid = ntohll(command.attr[0].val);
	osd_command_attr_free(&command);
	return oid;
}

static void obj_remove(int fd, uint64_t pid, uint64_t oid)
{
	struct osd_command command;
	int ret;

	osd_command_set_remove(&command, pid, oid);
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit_and_wait failed", __func__);
		exit(1);
	}
}

static void read_bw(int fd, uint64_t pid, uint64_t oid,
		    size_t sz, int iters, int dosync)
{
	int i = 0;
	int ret = 0;
	uint64_t start, end, delta;
	double mhz = get_mhz();
	double time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;

	buf = malloc(sz);
	b = malloc(iters * sizeof(*b));

	if (!buf || !b)
		osd_error_fatal("out of memory");

	/* warm up */
	for (i=0; i< 5; i++) {
		ret = read_osd(fd, pid, oid, buf, sz, 0);
		assert(ret == 0);
	}

	for (i=0; i< iters; i++) {
		if (dosync) {
			rdtsc(start);
			ret = read_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;

			rdtsc(start);
			ret = flush_object(fd, pid, oid, 2);
			rdtsc(end);
			assert(ret == 0);
			delta += (end - start);
		} else {
			rdtsc(start);
			ret = read_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;
		}

		time = ((double)delta)/mhz; /* time in usec */
		b[i] = sz/time; /* BW in MegaBytes/sec */
	}

	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	if (rank == 0) {
		if (dosync)
			printf("read-sync  %3lu %7.3lf +- %7.3lf\n",
			       sz>>10, mu, sd);
		else
			printf("read       %3lu %7.3lf +- %7.3lf\n",
			       sz>>10, mu, sd);
	}

	free(buf);
	free(b);
}

static void write_bw(int fd, uint64_t pid, uint64_t oid,
		     size_t sz, int iters, int dosync)
{
	int i = 0;
	int ret = 0;
	uint64_t start, end, delta;
	double mhz = get_mhz();
	double time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;

	buf = malloc(sz);
	b = malloc(iters * sizeof(*b));

	if (!buf || !b)
		osd_error_fatal("out of memory");

	/* warm up */
	for (i=0; i< 5; i++) {
		ret = write_osd(fd, pid, oid, buf, sz, 0);
		assert(ret == 0);
	}

	for (i=0; i< iters; i++) {
		if (dosync) {
			rdtsc(start);
			ret = write_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;

			rdtsc(start);
			/* XXX: this may be broken, numbers reported
			 * are too big to be reasonable */
			ret = flush_object(fd, pid, oid, 2);
			rdtsc(end);
			assert(ret == 0);

			delta += end - start;
		} else {
			rdtsc(start);
			ret = write_osd(fd, pid, oid, buf, sz, 0);
			rdtsc(end);
			assert(ret == 0);
			delta = end - start;
		}

		time = ((double)delta)/mhz; /* time in usec */
		b[i] = sz/time; /* BW in MegaBytes/sec */
	}

	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	if (rank == 0) {
		if (dosync)
			printf("write-sync %3lu %7.3lf +- %7.3lf\n",
			       sz>>10, mu, sd);
		else
			printf("write      %3lu %7.3lf +- %7.3lf\n",
			       sz>>10, mu, sd);
	}

	free(buf);
	free(b);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;
	const int iter = 500;
	uint64_t oid;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numproc);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
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
	osd_debug("%s: drive %s name %s", progname, drives[i].chardev,
		  drives[i].targetname);
	fd = open(drives[i].chardev, O_RDWR);
	if (fd < 0) {
		osd_error_errno("%s: open %s", __func__, drives[i].chardev);
		return 1;
	}
	osd_free_drive_list(drives, num_drives);

	inquiry(fd);

	if (rank == 0) {
		format_osd(fd, 1<<30);
		create_partition(fd, PARTITION_PID_LB);
	}
	MPI_Barrier(MPI_COMM_WORLD);

	/* each works on a different object */
	oid = obj_create_any(fd, PARTITION_PID_LB);
	if (rank == 0) {
		printf("# osd_initiator/tests/iospeed\n");
		printf("# type  size (kB)  rate (MB/s) +- stdev\n");
	}
	for (i=4096; i<=(1<<19); i+=4096) {
		MPI_Barrier(MPI_COMM_WORLD);
		write_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
	}
	if (rank == 0) {
		printf("\n\n");
	}
	for (i=4096; i<=(1<<19); i+=4096) {
		MPI_Barrier(MPI_COMM_WORLD);
		read_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
	}
	if (rank == 0) {
		printf("\n\n");
	}
#if 0
	for (i=4096; i<=(1<<19); i+=4096)
		write_bw(fd, PARTITION_PID_LB, oid, i, iter, 1);
	if (rank == 0) {
		printf("\n\n");
	}
	for (i=4096; i<=(1<<19); i+=4096)
		read_bw(fd, PARTITION_PID_LB, oid, i, iter, 1);
	if (rank == 0) {
		printf("\n\n");
	}
#endif

	obj_remove(fd, PARTITION_PID_LB, oid);

	close(fd);
	MPI_Finalize();
	return 0;
}

