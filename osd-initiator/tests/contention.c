/*
 * contention tests
 *
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * string for test,
 * clean code,
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include <mpi.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

static int rank, numprocs;
static const int WORK_MAX = 600;
static const int test_lb = 1;
static const int test_ub = 18;

static void usage(void)
{
	osd_error_fatal("Usage: ./%s {test}\n"
			"\t{test} must belong to set [%d, %d]", 
			osd_get_progname(), test_lb, test_ub);
}

static void println(const char *fmt, ...)
{
	va_list ap;

	fprintf(stdout, "%d: ", rank);
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fprintf(stdout, ".\n");
}

static inline void local_work(void) 
{
	usleep(1 + (int)(rand()*WORK_MAX/(RAND_MAX+1.)));
}

static void remote_work(int fd, uint64_t pid, uint64_t oid) 
{
	int ret;
	struct osd_command cmd;
	struct attribute_list attr = {
		.type = ATTR_GET,
		.page = USER_INFO_PG,
		.number = UIAP_LOGICAL_LEN,
		.len = 8,
	};

	osd_command_set_get_attributes(&cmd, pid, oid);
	osd_command_attr_build(&cmd, &attr, 1);
	ret = osd_submit_and_wait(fd, &cmd);
	assert(ret == 0);
	osd_command_attr_resolve(&cmd);
}

/*
 * return values
 * -1: error
 *  0: cas failed
 *  1: cas succeeded
 */
static int cas(int fd, struct osd_command *cmd, uint64_t pid, uint64_t oid,
	       uint64_t cmp, uint64_t swap)
{
	int ret;
	uint64_t outbuf[2], inbuf[1];

	osd_command_set_cas(cmd, pid, oid, 8, 0);
	cmd->outdata = outbuf;
	cmd->outlen = sizeof(outbuf);
	cmd->indata = inbuf;
	cmd->inlen_alloc = sizeof(inbuf);

	inbuf[0] = 0xdeadbeef;
	set_htonll((uint8_t *)&outbuf[0], cmp);
	set_htonll((uint8_t *)&outbuf[1], swap);
	ret = osd_submit_and_wait(fd, cmd);
	if (ret != 0)
		return -1;
	if (get_ntohll((uint8_t *)inbuf) == get_ntohll((uint8_t *)outbuf))
		return 1;
	else
		return 0;
}


static int fa(int fd, struct osd_command *cmd, uint64_t pid, uint64_t oid,
	      int64_t add, int64_t *orig)
{
	int ret;
	uint64_t outbuf[1], inbuf[1];

	osd_command_set_fa(cmd, pid, oid, 8, 0);
	cmd->outdata = outbuf;
	cmd->outlen = sizeof(outbuf);
	cmd->indata = inbuf;
	cmd->inlen_alloc = sizeof(inbuf);

	inbuf[0] = 0xdeadbeef;
	set_htonll((uint8_t *)&outbuf[0], add);
	ret = osd_submit_and_wait(fd, cmd);
	if (ret != 0)
		return -1;
	*orig = get_ntohll((uint8_t *)inbuf);
	return 0;
}


static void busy_wait(int fd, uint64_t pid, uint64_t oid, const int numlocks,
		      const int dowork)
{
	int i;
	int ret;
	int reqs, attempts;
	int locks = numlocks;
	double *global_req, *global_att;
	int *gather;
	uint64_t start, end;
	double mu_req, sd_req, mu_att, sd_att;
	double time, mhz = get_mhz();
	struct osd_command cmd;
	
	if (rank == 0) {
		gather = calloc(numprocs, sizeof(*gather));
		global_att = calloc(numprocs, sizeof(*global_att));
		global_req = calloc(numprocs, sizeof(*global_req));
		if (!gather || !global_req || !global_att) {
			osd_error_xerrno(-ENOMEM, "out of memory");
			exit(1);
		}

	}

	if (dowork)
		osd_srand();

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0)
		rdtsc(start);

	reqs = 0, attempts = 0;
	while (locks) {
		ret = cas(fd, &cmd, pid, oid, 0, 1); /* lock */
		++attempts;
		++reqs;
		if (ret == 1) {
			--locks;
			if (dowork == 1)
				local_work();
			else if (dowork == 2)
				remote_work(fd, pid, oid);
			ret = cas(fd, &cmd, pid, oid, 1, 0); /* unlock */
			assert(ret == 1);
			++reqs;
		} else if (ret == 0) {
			continue;
		} else {
			osd_error_fatal("cas error");
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0) {
		rdtsc(end);
		time = ((double)(end-start))/mhz;
	}

	ret = MPI_Gather(&reqs, 1, MPI_INT, gather, 1, MPI_INT, 0,
			 MPI_COMM_WORLD);
	if (ret != 0) {
		osd_error("MPI_Gather failed");
		exit(1);
	}
	if (rank == 0)
		for (i = 0; i < numprocs; i++)
			global_req[i] = (double)gather[i];
	ret = MPI_Gather(&attempts, 1, MPI_INT, gather, 1, MPI_INT, 0,
			 MPI_COMM_WORLD);
	if (ret != 0) {
		osd_error("MPI_Gather failed");
		exit(1);
	}
	if (rank == 0)
		for (i = 0; i < numprocs; i++)
			global_att[i] = (double)gather[i];

	if (rank != 0)
		return;
	mu_req = mean(global_req, numprocs);
	sd_req = stddev(global_req, mu_req, numprocs);
	mu_att = mean(global_att, numprocs);
	sd_att = stddev(global_att, mu_att, numprocs);

	for (i = 0; i < numprocs; i++)
		printf("global_reqs[%u] = %lf, global_att[%u] = %lf\n", i, 
		       global_req[i], i, global_att[i]);
	printf("Numlocks: %u \n"
	       "Numprocs: %u \n"
	       "Work: %u \n"
	       "Total requests %7.3lf \n"
	       "Total attempts %7.3lf \n"
	       "Per node attempts: %6.3lf +- %6.3lf \n"
	       "Efficiency: %7.3lf \n"
	       "Time %7.3lf us\n"
	       "Throughput: %7.3lf locks/sec\n", 
	       numlocks, numprocs, dowork, mu_req*numprocs, mu_att*numprocs,
	       mu_att, sd_att, numlocks*2/mu_req, time,
	       numlocks*numprocs*1e6/time);

	free(gather);
	free(global_req);
	free(global_att);
}


static void buggy(int fd, uint64_t pid, uint64_t oid, int numlocks,
		  int dowork, int *my_att, int *my_req, double *latency)
{
	int ret;
	int reqs, attempts;
	int locks = 0;
	int64_t num_waiters, queued;
	int first_attempt;
	double rtt, mhz = get_mhz();
	uint64_t start, end, lat_beg, lat_end;
	struct osd_command cmd;

	reqs = 0, attempts = 0, num_waiters = 0, queued = 0, first_attempt = 1;
	while (locks < numlocks) {
		if (first_attempt) {
			rdtsc(lat_beg);
			first_attempt = 0;
		}
		ret = cas(fd, &cmd, pid, oid, 0, 1); /* lock */
		++attempts;
		++reqs;
		if (ret == 1) {
			rdtsc(lat_end);
			latency[locks] = ((double)(lat_end - lat_beg)/mhz);
			first_attempt = 1;
			if (queued) {
				ret = fa(fd, &cmd, pid, oid, -1,
					 &num_waiters);
				if (ret == -1)
					osd_error_fatal("fa failed");
				++reqs;
				--queued;
			}
			++locks;
			if (dowork == 1) 
				local_work();
			else if (dowork == 2)
				remote_work(fd, pid, oid);
			ret = cas(fd, &cmd, pid, oid, 1, 0); /* unlock */
			assert(ret == 1);
			++reqs;
		} else if (ret == 0) {
			rdtsc(start);
			ret = fa(fd, &cmd, pid, oid, 1, &num_waiters);
			rdtsc(end);
			if (ret == -1)
				osd_error_fatal("fa error");
			++reqs;
			++queued;
			if (num_waiters == 0)
				continue;

			/* speculative delay estimation */
			rtt = ((double)(end-start))/mhz;
			//println("num waiters %u rtt %7.3lf", num_waiters, rtt);
			if (dowork == 0) {
				usleep((uint64_t)((num_waiters-1)*(rtt) 
						  + rtt/2));
			} else if (dowork == 1) {
				usleep((uint64_t)((num_waiters-1)*(rtt)
						  + rtt/2 + WORK_MAX/2.)); 
			} else if (dowork == 2) {
				usleep((uint64_t)((num_waiters)*(rtt)
						  + rtt/2)); 
			}
			continue;
		} else {
			osd_error_fatal("cas error");
		}
	}
	*my_att = attempts;
	*my_req = reqs;
}

static void goback(int fd, uint64_t pid, uint64_t oid, int numlocks,
		   int dowork, int *my_att, int *my_req, double *latency)
{
	int ret;
	int reqs, attempts;
	int locks = 0;
	int64_t num_waiters, queued;
	int first_attempt;
	double rtt, mhz = get_mhz();
	uint64_t start, end, lat_beg, lat_end;
	struct osd_command cmd;

	reqs = 0, attempts = 0, num_waiters = 0, queued = 0, first_attempt = 1;
	while (locks < numlocks) {
		if (first_attempt) {
			rdtsc(lat_beg);
			first_attempt = 0;
		}
		ret = cas(fd, &cmd, pid, oid, 0, 1); /* lock */
		++attempts;
		++reqs;
		if (ret == 1) {
			rdtsc(lat_end);
			latency[locks] = ((double)(lat_end - lat_beg)/mhz);
			first_attempt = 1;
			if (queued) {
				ret = fa(fd, &cmd, pid, oid, -queued,
					 &num_waiters);
				if (ret == -1)
					osd_error_fatal("fa failed");
				++reqs;
				queued = 0;
			}
			++locks;
			if (dowork == 1) 
				local_work();
			else if (dowork == 2)
				remote_work(fd, pid, oid);
			ret = cas(fd, &cmd, pid, oid, 1, 0); /* unlock */
			assert(ret == 1);
			++reqs;
		} else if (ret == 0) {
			rdtsc(start);
			ret = fa(fd, &cmd, pid, oid, 1, &num_waiters);
			rdtsc(end);
			if (ret == -1)
				osd_error_fatal("fa error");
			++reqs;
			++queued;
			if (num_waiters == 0)
				continue;

			/* speculative delay estimation */
			rtt = ((double)(end-start))/mhz;
			//println("num waiters %u rtt %7.3lf", num_waiters, rtt);
			if (dowork == 0) {
				usleep((uint64_t)((num_waiters-1)*(rtt) 
						  + rtt/2));
			} else if (dowork == 1) {
				usleep((uint64_t)((num_waiters-1)*(rtt)
						  + rtt/2 + WORK_MAX/2.)); 
			} else if (dowork == 2) {
				usleep((uint64_t)((num_waiters)*(rtt)
						  + rtt/2)); 
			}
			continue;
		} else {
			osd_error_fatal("cas error");
		}
	}
	*my_att = attempts;
	*my_req = reqs;
}


static void spinpos(int fd, uint64_t pid, uint64_t oid, int numlocks,
		    int dowork, int *my_att, int *my_req, double *latency)
{
	int ret;
	int reqs, attempts;
	int locks = 0;
	int64_t num_waiters, queued;
	int first_attempt;
	double rtt, mhz = get_mhz();
	uint64_t start, end, lat_beg, lat_end;
	struct osd_command cmd;

	reqs = 0, attempts = 0, num_waiters = 0, queued = 0, first_attempt = 1;
	while (locks < numlocks) {
		if (first_attempt) {
			rdtsc(lat_beg);
			first_attempt = 0;
		}
		ret = cas(fd, &cmd, pid, oid, 0, 1); /* lock */
		++attempts;
		++reqs;
		if (ret == 1) {
			rdtsc(lat_end);
			latency[locks] = ((double)(lat_end - lat_beg)/mhz);
			first_attempt = 1;
			if (queued) {
				ret = fa(fd, &cmd, pid, oid, -1, &num_waiters);
				if (ret == -1)
					osd_error_fatal("fa failed");
				++reqs;
				queued = 0;
			}
			++locks;
			if (dowork == 1) 
				local_work();
			else if (dowork == 2)
				remote_work(fd, pid, oid);
			ret = cas(fd, &cmd, pid, oid, 1, 0); /* unlock */
			assert(ret == 1);
			++reqs;
		} else if (ret == 0) {
			if (!queued) {
				rdtsc(start);
				ret = fa(fd, &cmd, pid, oid, 1, &num_waiters);
				rdtsc(end);
				if (ret == -1)
					osd_error_fatal("fa error");
				++reqs;
				queued = 1;
			}
			if (num_waiters == 0)
				continue;

			/* speculative delay estimation */
			rtt = ((double)(end-start))/mhz;
			//println("num waiters %u rtt %7.3lf", num_waiters, rtt);
			if (dowork == 0) {
				usleep((uint64_t)((num_waiters-1)*(rtt) 
						  + rtt/2));
			} else if (dowork == 1) {
				usleep((uint64_t)((num_waiters-1)*(rtt)
						  + rtt/2 + WORK_MAX/2.)); 
			} else if (dowork == 2) {
				usleep((uint64_t)((num_waiters)*(rtt)
						  + rtt/2)); 
			}
			continue;
		} else {
			osd_error_fatal("cas error");
		}
	}
	*my_att = attempts;
	*my_req = reqs;
}


static void spec_idle(int fd, uint64_t pid, uint64_t oid, int numlocks,
		      int dowork, int test)
{
	int i;
	int ret;
	double *global_req, *global_att, *global_lat, *latency;
	int *gather;
	int reqs, attempts;
	uint64_t exp_beg, exp_end;
	double mu_req, sd_req, mu_att, sd_att;
	double exp_time, mhz = get_mhz();
	struct osd_command cmd;
	int64_t num_waiters;
	
	if (rank == 0) {
		gather = calloc(numprocs, sizeof(*gather));
		global_att = calloc(numprocs, sizeof(*global_att));
		global_req = calloc(numprocs, sizeof(*global_req));
		global_lat = calloc(numprocs*2, sizeof(*global_lat));
		if (!gather || !global_req || !global_att || !global_lat) {
			osd_error_xerrno(-ENOMEM, "out of memory");
			exit(1);
		}

		/* initialize lock counter */
		num_waiters = 0xdeadbeef;
		ret = fa(fd, &cmd, pid, oid, 0, &num_waiters);
		if (ret == -1)
			osd_error_fatal("fa failed");
		if (num_waiters != 0) {
			ret = fa(fd, &cmd, pid, oid, -1*num_waiters, 
				 &num_waiters);
			if (ret == -1)
				osd_error_fatal("fa failed");
		}
	}

	latency = calloc(numlocks, sizeof(*latency));
	if (!latency) {
		osd_error_xerrno(-ENOMEM, "out of memory");
		exit(1);
	}
	osd_srand(); /* seed PRNG */

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0)
		rdtsc(exp_beg);

	if (test == 1) {
		buggy(fd, pid, oid, numlocks, dowork, &attempts, &reqs, 
		      latency);
	} else if (test == 2) {
		goback(fd, pid, oid, numlocks, dowork, &attempts, &reqs,
		       latency);
	} else {
		spinpos(fd, pid, oid, numlocks, dowork, &attempts, &reqs,
		       latency);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0) {
		rdtsc(exp_end);
		exp_time = ((double)(exp_end - exp_beg))/mhz;
	}

	ret = MPI_Gather(&reqs, 1, MPI_INT, gather, 1, MPI_INT, 0,
			 MPI_COMM_WORLD);
	if (ret != 0) {
		osd_error("MPI_Gather failed");
		exit(1);
	}
	if (rank == 0)
		for (i = 0; i < numprocs; i++)
			global_req[i] = (double)gather[i];
	ret = MPI_Gather(&attempts, 1, MPI_INT, gather, 1, MPI_INT, 0,
			 MPI_COMM_WORLD);
	if (ret != 0) {
		osd_error("MPI_Gather failed");
		exit(1);
	}
	if (rank == 0)
		for (i = 0; i < numprocs; i++)
			global_att[i] = (double)gather[i];

	double stats[2];
	stats[0] = mean(latency, numlocks);
	stats[1] = stddev(latency, stats[0], numlocks);
	ret = MPI_Gather(stats, 2, MPI_DOUBLE, global_lat, 2, MPI_DOUBLE, 0,
			 MPI_COMM_WORLD);
	if (ret != 0) {
		osd_error("MPI_Gather failed");
		exit(1);
	}

	if (rank != 0)
		return;
	mu_req = mean(global_req, numprocs);
	sd_req = stddev(global_req, mu_req, numprocs);
	mu_att = mean(global_att, numprocs);
	sd_att = stddev(global_att, mu_att, numprocs);

	for (i = 0; i < numprocs; i++)
		println("global_reqs[%u] = %lf, global_att[%u] = %lf\n"
		       "lat[%d]: %8.3lf +- %8.3lf", i, global_req[i], i,
		       global_att[i], i, global_lat[2*i], global_lat[2*i+1]); 
	printf("Numlocks: %u \n"
	       "Numprocs: %u \n"
	       "Work: %u \n"
	       "Total requests %7.3lf \n"
	       "Total attempts %7.3lf \n"
	       "Per node reqs: %6.3lf +- %6.3lf \n"
	       "Per node attempts: %6.3lf +- %6.3lf \n"
	       "Efficiency: %7.3lf \n"
	       "Time %7.3lf us\n"
	       "Throughput: %7.3lf locks/sec\n", 
	       numlocks, numprocs, dowork, mu_req*numprocs, mu_att*numprocs,
	       mu_req, sd_req, mu_att, sd_att, numlocks*2/mu_req, exp_time,
	       numlocks*numprocs*1e6/exp_time);

	free(gather);
	free(global_req);
	free(global_att);
}


static void lin_backoff(int fd, uint64_t pid, uint64_t oid, 
			const int numlocks, const int dowork)
{
	int i;
	int ret;
	int reqs, attempts;
	int locks = numlocks;
	double *global_req, *global_att;
	int *gather;
	uint64_t exp_beg, exp_end, start, end;
	double mu_req, sd_req, mu_att, sd_att;
	double rtt, exp_time, mhz = get_mhz();
	struct osd_command cmd;
	int failures;
	
	if (rank == 0) {
		gather = calloc(numprocs, sizeof(*gather));
		global_att = calloc(numprocs, sizeof(*global_att));
		global_req = calloc(numprocs, sizeof(*global_req));
		if (!gather || !global_req || !global_att) {
			osd_error_xerrno(-ENOMEM, "out of memory");
			exit(1);
		}
	}

	osd_srand(); /* seed PRNG */

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0)
		rdtsc(exp_beg);

	reqs = 0, attempts = 0, failures = 0;
	while (locks) {
		rdtsc(start);
		ret = cas(fd, &cmd, pid, oid, 0, 1); /* lock */
		rdtsc(end);
		++attempts;
		++reqs;
		if (ret == 1) {
			--locks;
			failures = 0;
			if (dowork == 1) 
				local_work();
			else if (dowork == 2)
				remote_work(fd, pid, oid);
			ret = cas(fd, &cmd, pid, oid, 1, 0); /* unlock */
			assert(ret == 1);
			++reqs;
		} else if (ret == 0) {
			++failures;
			rtt = ((double)(end - start)/mhz);
			usleep((uint64_t)failures*rtt/2.);
			continue;
		} else {
			osd_error_fatal("cas error");
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0) {
		rdtsc(exp_end);
		exp_time = ((double)(exp_end - exp_beg))/mhz;
	}

	ret = MPI_Gather(&reqs, 1, MPI_INT, gather, 1, MPI_INT, 0,
			 MPI_COMM_WORLD);
	if (ret != 0) {
		osd_error("MPI_Gather failed");
		exit(1);
	}
	if (rank == 0)
		for (i = 0; i < numprocs; i++)
			global_req[i] = (double)gather[i];
	ret = MPI_Gather(&attempts, 1, MPI_INT, gather, 1, MPI_INT, 0,
			 MPI_COMM_WORLD);
	if (ret != 0) {
		osd_error("MPI_Gather failed");
		exit(1);
	}
	if (rank == 0)
		for (i = 0; i < numprocs; i++)
			global_att[i] = (double)gather[i];

	if (rank != 0)
		return;
	mu_req = mean(global_req, numprocs);
	sd_req = stddev(global_req, mu_req, numprocs);
	mu_att = mean(global_att, numprocs);
	sd_att = stddev(global_att, mu_att, numprocs);

	for (i = 0; i < numprocs; i++)
		printf("global_reqs[%u] = %lf, global_att[%u] = %lf\n", i, 
		       global_req[i], i, global_att[i]);
	printf("Numlocks: %u \n"
	       "Numprocs: %u \n"
	       "Work: %u \n"
	       "Total requests %7.3lf \n"
	       "Total attempts %7.3lf \n"
	       "Per node attempts: %6.3lf +- %6.3lf \n"
	       "Efficiency: %7.3lf \n"
	       "Time %7.3lf us\n"
	       "Throughput: %7.3lf locks/sec\n", 
	       numlocks, numprocs, dowork, mu_req*numprocs, mu_att*numprocs,
	       mu_att, sd_att, numlocks*2/mu_req, exp_time,
	       numlocks*numprocs*1e6/exp_time);

	free(gather);
	free(global_req);
	free(global_att);
}
int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;
	uint64_t pid = USEROBJECT_PID_LB, oid = USEROBJECT_OID_LB;
	struct osd_command cmd;
	int test;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	osd_set_progname(argc, argv);

	if (argc != 2)
		usage();

	test = strtol(argv[1], NULL, 10);
	assert(test_lb <= test && test <= test_ub);

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


	switch (test) {
	case 1: /* contention no work */
	case 2: /* contention with local work */
	case 3: /* contention with remote work */
		if (rank == 0) {
			osd_command_set_format_osd(&cmd, 1<<30);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create(&cmd, pid, oid, 1);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}
		if (test == 1)
			busy_wait(fd, pid, oid, 100, 0);
		else if (test == 2)
			busy_wait(fd, pid, oid, 100, 1);
		else if (test == 3)
			busy_wait(fd, pid, oid, 100, 2);
		if (rank == 0) {
			osd_command_set_remove(&cmd, pid, oid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_remove_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}
		break;
	case 4: /* No contention, no work */
	case 5: /* No contention, local work */
	case 6: /* No contention, remote work */
		if (rank == 0) {
			osd_command_set_format_osd(&cmd, 1<<30);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			for (i = oid; (uint32_t)i < oid+numprocs; i++) {
				osd_command_set_create(&cmd, pid, i, 1);
				ret = osd_submit_and_wait(fd, &cmd);
				assert(ret == 0);
			}
		}
		if (test == 4)
			busy_wait(fd, pid, oid+rank, 100, 0);
		else if (test == 5)
			busy_wait(fd, pid, oid+rank, 100, 1);
		else if (test == 6)
			busy_wait(fd, pid, oid+rank, 100, 2);

		if (rank == 0) {
			for (i = oid; (uint32_t)i < oid+numprocs; i++) {
				osd_command_set_remove(&cmd, pid, i);
				ret = osd_submit_and_wait(fd, &cmd);
				assert(ret == 0);
			}

			osd_command_set_remove_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}
		break;
	case 7: /* Contention, speculative idling, no work */
	case 8: /* Contention, speculative idling, local work */
	case 9: /* Contention, speculative idling, remote work */
		if (rank == 0) {
			osd_command_set_format_osd(&cmd, 1<<30);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create(&cmd, pid, oid, 1);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}

		if (test == 7)
			spec_idle(fd, pid, oid, 100, 0, 1);
		else if (test == 8)
			spec_idle(fd, pid, oid, 100, 1, 1);
		else if (test == 9)
			spec_idle(fd, pid, oid, 100, 2, 1);
		
		if (rank == 0) {
			osd_command_set_remove(&cmd, pid, oid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_remove_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}
		break;
	case 10: /* Contention, lin backoff, no work */
	case 11: /* Contention, lin backoff, local work */
	case 12: /* Contention, lin backoff, remote work */
		if (rank == 0) {
			osd_command_set_format_osd(&cmd, 1<<30);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create(&cmd, pid, oid, 1);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}

		if (test == 10)
			lin_backoff(fd, pid, oid, 100, 0);
		else if (test == 11)
			lin_backoff(fd, pid, oid, 100, 1);
		else if (test == 12)
			lin_backoff(fd, pid, oid, 100, 2);
		
		if (rank == 0) {
			osd_command_set_remove(&cmd, pid, oid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_remove_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}
		break;
	case 13: /* Contention, speculative idling, goback no work */
	case 14: /* Contention, speculative idling, goback local work */
	case 15: /* Contention, speculative idling, goback remote work */
		if (rank == 0) {
			osd_command_set_format_osd(&cmd, 1<<30);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create(&cmd, pid, oid, 1);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}

		if (test == 13)
			spec_idle(fd, pid, oid, 100, 0, 2);
		else if (test == 14)
			spec_idle(fd, pid, oid, 100, 1, 2);
		else if (test == 15)
			spec_idle(fd, pid, oid, 100, 2, 2);
		
		if (rank == 0) {
			osd_command_set_remove(&cmd, pid, oid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_remove_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}
		break;
	case 16: /* Contention, speculative idling, spinpos no work */
	case 17: /* Contention, speculative idling, spinpos local work */
	case 18: /* Contention, speculative idling, spinpos remote work */
		if (rank == 0) {
			osd_command_set_format_osd(&cmd, 1<<30);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_create(&cmd, pid, oid, 1);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}

		if (test == 16)
			spec_idle(fd, pid, oid, 100, 0, 3);
		else if (test == 17)
			spec_idle(fd, pid, oid, 100, 1, 3);
		else if (test == 18)
			spec_idle(fd, pid, oid, 100, 2, 3);
		
		if (rank == 0) {
			osd_command_set_remove(&cmd, pid, oid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);

			osd_command_set_remove_partition(&cmd, pid);
			ret = osd_submit_and_wait(fd, &cmd);
			assert(ret == 0);
		}
		break;

	default:
		usage();
	}

	close(fd);
	MPI_Finalize();
	return 0;
}
