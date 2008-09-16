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

#include "osd-util/osd-util.h"
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
	oid = get_ntohll(command.attr[0].val);
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
	uint64_t start, end, delta, total_start, total_stop;
	double mhz = get_mhz();
	double time = 0.0;
	double max_time = 0.0;
	double min_time = 0.0;
	double mu, sd, total;
	double *b = NULL;
	void *buf = NULL;
	size_t total_size;

	buf = malloc(sz);
	b = malloc(iters * sizeof(*b));

	if (!buf || !b)
		osd_error_fatal("out of memory");

	/* warm up */
	if (iters > 5)
		for (i=0; i<5; i++) {
			ret = read_osd(fd, pid, oid, buf, sz, 0);
			assert(ret == 0);
		}

	memset(buf, '\0', sz);

	MPI_Barrier(MPI_COMM_WORLD);
	rdtsc(total_start);
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

	MPI_Barrier(MPI_COMM_WORLD); /*everyone is done reading*/
	rdtsc(total_stop);

	unsigned int j;
	for (j=0; j<sz; j++) {
		char *c = (char *)buf + j;
		if (*c != 'D') {
			printf("[%d] ERROR READING BUFF (%c)\n", rank, *c);
		}
	}

#if 1
	delta = total_stop - total_start;
	time = ((double)delta)/mhz; /*time in usec*/

	ret = MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX,
			0, MPI_COMM_WORLD);
	if (ret != MPI_SUCCESS) {
		printf("MPI ERROR\n");
	}

	ret = MPI_Reduce(&time, &min_time, 1, MPI_DOUBLE, MPI_MIN,
			0, MPI_COMM_WORLD);
	if (ret != MPI_SUCCESS) {
		printf("MPI ERROR\n");
	}
	if (rank == 0) {
		total_size = sz * iters * numproc; /*total bytes moved*/
		printf("read  %3d %3lu %7.3lf --- Discrep %.0f is %.1f%%\n",
			numproc, sz>>10, total_size/max_time,
			max_time - min_time, 100. * (max_time - min_time) / max_time);
	}
#else
	mu = mean(b, iters);
	sd = stddev(b, mu, iters);

	total = mu * numproc;

	if (dosync)
		printf("rank %d read-sync  %3lu %7.3lf +- %7.3lf\n",
		       rank, sz>>10, mu, sd);
	else
		printf("rank %d read       %3lu %7.3lf +- %7.3lf\n",
		       rank, sz>>10, mu, sd);
#endif
	free(buf);
	free(b);
}

static void write_bw(int fd, uint64_t pid, uint64_t oid,
		     size_t sz, int iters, int dosync)
{
	int i = 0;
	int ret = 0;
	uint64_t start, end, delta, total_start, total_stop;;
	double mhz = get_mhz();
	double time = 0.0;
	double max_time = 0.0;
	double min_time = 0.0;
	double mu, sd;
	double *b = NULL;
	void *buf = NULL;
	size_t total_size;

	buf = malloc(sz);
	b = malloc(iters * sizeof(*b));

	if (!buf || !b)
		osd_error_fatal("out of memory");

	/* warm up */
	if (iters > 5)
		for (i=0; i<5; i++) {
			ret = write_osd(fd, pid, oid, buf, sz, 0);
			assert(ret == 0);
		}

	memset(buf, 'D', sz);

	MPI_Barrier(MPI_COMM_WORLD);
	rdtsc(total_start);
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
	MPI_Barrier(MPI_COMM_WORLD); /*everyone is done reading*/
	rdtsc(total_stop);

#if 1
	delta = total_stop - total_start;
	time = ((double)delta)/mhz; /*time in usec*/

	ret = MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX,
			0, MPI_COMM_WORLD);
	if (ret != MPI_SUCCESS) {
		printf("MPI ERROR\n");
	}
	ret = MPI_Reduce(&time, &min_time, 1, MPI_DOUBLE, MPI_MIN,
			0, MPI_COMM_WORLD);
	if (ret != MPI_SUCCESS) {
		printf("MPI ERROR\n");
	}
	if (rank == 0) {
		total_size = sz * iters * numproc; /*total bytes moved*/
		printf("write %3d %3lu %7.3lf --- Discrep %.0f is %.1f%%\n",
			numproc, sz>>10, total_size/max_time,
			max_time - min_time, 100. * (max_time - min_time) / max_time);
	}
#else
	mu = mean(b, iters);
	sd = stddev(b, mu, iters);
	if (dosync)
		printf("rank %d write-sync %3lu %7.3lf +- %7.3lf\n",
		       rank, sz>>10, mu, sd);
	else
		printf("rank %d write      %3lu %7.3lf +- %7.3lf\n",
		       rank, sz>>10, mu, sd);
#endif
	free(buf);
	free(b);
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [<size in kbytes>]\n", osd_get_progname());
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;
	const int iter = 1000;
	uint64_t oid;
	int onesize = 0;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numproc);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	osd_set_progname(argc, argv);

	if (argc == 1) {
		;
	} else if (argc == 2) {
		char *cp;
		onesize = strtoul(argv[1], &cp, 0);
		if (*cp != '\0')
			usage();
		onesize <<= 10;
	} else {
		usage();
	}


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
	osd_debug("%s: drive %s name %s", osd_get_progname(), drives[i].chardev,
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

	if (onesize) {
		if (rank == 0) {
			printf("# osd_initiator/tests/iospeed %d\n",
			       onesize>>10);
			printf("# type  numproc  size (kB)  rate (MB/s) --- time diff (us)\n");
		}
		MPI_Barrier(MPI_COMM_WORLD);
		write_bw(fd, PARTITION_PID_LB, oid, onesize, iter, 0);
		MPI_Barrier(MPI_COMM_WORLD);
		read_bw(fd, PARTITION_PID_LB, oid, onesize, iter, 0);
	} else {
		if (rank == 0) {
			printf("# osd_initiator/tests/iospeed\n");
			printf("# type  size (kB)  rate (MB/s) +- stdev\n");
		}
		for (i=4096; i<=(1<<19); i+=4096) {
			MPI_Barrier(MPI_COMM_WORLD);
			write_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
		}
		for (i=4096; i<=(1<<19); i+=4096) {
			MPI_Barrier(MPI_COMM_WORLD);
			read_bw(fd, PARTITION_PID_LB, oid, i, iter, 0);
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
	}

	obj_remove(fd, PARTITION_PID_LB, oid);

	close(fd);
	MPI_Finalize();
	return 0;
}

