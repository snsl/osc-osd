#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "util/osd-defs.h"
#include "osd-types.h"
#include "osd.h"
#include "db.h"
#include "coll.h"
#include "util/util.h"

static void time_insert(struct osd_device *osd, int numobj, int numiter, 
			int testone)
{
	int ret = 0;
	int i = 0, j = 0;
	uint64_t start, end;
	double *t = 0;
	double mu, sd;

	t = Malloc(sizeof(*t) * numiter);
	if (!t)
		return;

	ret = coll_insert(osd->dbc, 1, 1, 2, 1);
	assert(ret == 0);
	ret = coll_delete(osd->dbc, 1, 1, 2);
	assert(ret == 0);

	if (testone == 0) {
		for (i = 0; i < numiter; i++) {
			t[i] = 0.0;
			for (j = 0; j < numobj; j++) {
				rdtsc(start);
				ret = coll_insert(osd->dbc, 1, 1, j, 1);
				rdtsc(end);
				assert(ret == 0);

				t[i] += (double)(end - start) / mhz;
			}

			ret = coll_remove_cid(osd->dbc, 1, 1);
			assert (ret == 0);
		}
	} else if (testone == 1) {
		for (i = 0; i < numobj; i++) {
			ret = coll_insert(osd->dbc, 1, 1, i, 1);
			assert (ret == 0);
		}
		
		for (i = 0; i < numiter; i++) {
			rdtsc(start);
			ret = coll_insert(osd->dbc, 1, 2, 1, 1);
			rdtsc(end);
			assert(ret == 0);

			ret = coll_delete(osd->dbc, 1, 2, 1);
			assert(ret == 0);

			t[i] = (double)(end - start) / mhz;
		}

		ret = coll_remove_cid(osd->dbc, 1, 1);
		assert (ret == 0);
	}

	mu = mean(t, numiter);
	sd = stddev(t, mu, numiter);
	printf("time_insert numiter %d numobj %d testone %d avg %lf +- %lf"
	       " us\n", numiter, numobj, testone, mu, sd);
}

static void time_delete(struct osd_device *osd, int numobj, int numiter, 
			int testone)
{
	int ret = 0;
	int i = 0, j = 0;
	uint64_t start, end;
	double *t = 0;
	double mu, sd;

	t = Malloc(sizeof(*t) * numiter);
	if (!t)
		return;

	ret = coll_insert(osd->dbc, 1, 1, 2, 1);
	assert(ret == 0);
	ret = coll_delete(osd->dbc, 1, 1, 2);
	assert(ret == 0);

	if (testone == 0) {
		for (i = 0; i < numiter; i++) {
			for (j = 0; j < numobj; j++) {
				ret = coll_insert(osd->dbc, 1, 1, j, 1);
				assert(ret == 0);
			}

			t[i] = 0;
			for (j = 0; j < numobj; j++) {
				rdtsc(start);
				ret = coll_delete(osd->dbc, 1, 1, j);
				rdtsc(end);
				assert(ret == 0);

				t[i] += (double)(end - start) / mhz;
			}

		}
	} else if (testone == 1) {
		for (i = 0; i < numobj; i++) {
			ret = coll_insert(osd->dbc, 1, 1, i, 1);
			assert (ret == 0);
		}
		
		for (i = 0; i < numiter; i++) {
			ret = coll_insert(osd->dbc, 1, 2, 1, 1);
			assert(ret == 0);

			rdtsc(start);
			ret = coll_delete(osd->dbc, 1, 2, 1);
			rdtsc(end);
			assert(ret == 0);

			t[i] = (double)(end - start) / mhz;
		}

		ret = coll_remove_cid(osd->dbc, 1, 1);
		assert (ret == 0);
	}

	mu = mean(t, numiter);
	sd = stddev(t, mu, numiter);
	printf("time_delete numiter %d numobj %d testone %d avg %lf +- %lf"
	       " us\n", numiter, numobj, testone, mu, sd);
}

static void usage(void)
{
	fprintf(stderr, "Usage: ./%s [-o <numobj>] [-i <numiter>]"
		" [-t <func-to-be-timed>]\n", progname);
	fprintf(stderr, "Option -t takes following values:\n");
	fprintf(stderr, "%12s: cumulative time for insert\n", "insert");
	fprintf(stderr, "%12s: cumulative time for delete\n", "delete");
	fprintf(stderr, "%12s: time to insert one after 'o'\n", "insertone");
	fprintf(stderr, "%12s: time to delete one after 'o'\n", "deleteone");
	exit(1);
}

int main(int argc, char **argv)
{
	int ret = 0;
	int numobj = 10;
	int numiter = 10;
	const char *root = "/tmp/osd/";
	const char *func = NULL;
	struct osd_device osd;

	osd_set_progname(argc, argv);
	while (++argv, --argc > 0) {
		const char *s = *argv;
		if (s[0] == '-') {
			switch (s[1]) {
			case 'i':
				++argv, --argc;
				if (argc < 1)
					usage();
				numiter = atoi(*argv);
				break;
			case 'o':
				++argv, --argc;
				if (argc < 1)
					usage();
				numobj = atoi(*argv);
				break;
			case 't':
				++argv, --argc;
				if (argc < 1)
					usage();
				func = *argv;
				break;
			default:
				usage();
			}
		}
	}

	if (!func)
		usage();

	system("rm -rf /tmp/osd/*");
	ret = osd_open(root, &osd);
	assert(ret == 0);

  	if (!strcmp(func, "insert")) {
		time_insert(&osd, numobj, numiter, 0);
	} else if (!strcmp(func, "insertone")) {
		time_insert(&osd, numobj, numiter, 1);
	} else if (!strcmp(func, "delete")) {
		time_delete(&osd, numobj, numiter, 0);
	} else if (!strcmp(func, "deleteone")) {
		time_delete(&osd, numobj, numiter, 1);
	} else {
		usage();
	} 

	ret = osd_close(&osd);
	assert(ret == 0);

	return 0;
}
