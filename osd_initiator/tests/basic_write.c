/*
 * Test the chardev transport to the kernel.
 */

#ifndef __KERNEL__
typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

#define BITS_PER_LONG 32 

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/types.h>
#include <scsi/scsi.h>
#include <libosd/interface.h>
#include <libosd/util.h>
#include <libosd/osd_hdr.h>

#define VARLEN_CDB_SIZE 200
#define VARLEN_CDB 0x7f

/*
 * Initializes a new varlen cdb.
 */
static void varlen_cdb_init(uint8_t *cdb)
{
	int i;

	/* memset(cdb, 0, VARLEN_CDB_SIZE); */
	for (i=0; i<VARLEN_CDB_SIZE; i++)
		cdb[i] = i;
	cdb[0] = VARLEN_CDB;
	cdb[7] = VARLEN_CDB_SIZE - 8;  /* total length */
	cdb[11] = 2 << 4;  /* get/set attributes page format */
}

static void cdb_build_inquiry(uint8_t *cdb)
{
	memset(cdb, 0, 6);
	cdb[0] = INQUIRY;
	cdb[4] = 80;
}

static void hexdump(uint8_t *d, size_t len)
{
	size_t offset = 0;

	while (offset < len) {
		int i, range;

		range = 8;
		if (range > len-offset)
			range = len-offset;
		printf("%4lx:", offset);
		for (i=0; i<range; i++)
			printf(" %02x", d[offset+i]);
		printf("\n");
		offset += range;
	}
}

int main(int argc, char *argv[])
{
	uint8_t cdb[200];
	uint8_t inquiry_rsp[80];
	uint64_t key;
	int fd, err, i;

	set_progname(argc, argv);

	fd = uosd_open("/dev/sua");

#if 0
	for (i=0; i<2; i++)
	{
		info("inquiry");
		memset(inquiry_rsp, 0xaa, 80);
		cdb_build_inquiry(cdb);
		memset(inquiry_rsp, 0, sizeof(inquiry_rsp));
		uosd_cdb_read(fd, cdb, 6, inquiry_rsp, sizeof(inquiry_rsp));

		/* now wait for the result */
		info("waiting for response");
		err = uosd_wait_response(fd, &key);
		info("response key %lx error %d", key, err);
		hexdump(inquiry_rsp, sizeof(inquiry_rsp));
		fflush(0);	
	}

	info("sleeping 2 before flush");
	sleep(2);

	info("osd flush osd");
	varlen_cdb_init(cdb);
	osd_flush_osd(cdb, 2);   /* flush everything */
	uosd_cdb_nodata(fd, cdb, 200);
	info("waiting for response");
	err = uosd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	info("sleeping 2 before format");
	sleep(2);
#endif

	info("osd format osd");
	varlen_cdb_init(cdb);
	osd_format_osd(cdb, 1<<30);   /* capacity 1 GB */
	uosd_cdb_nodata(fd, cdb, 200);
	err = uosd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	info("osd create");
	varlen_cdb_init(cdb);
	osd_create(cdb, 0, 27, 1);
	uosd_cdb_nodata(fd, cdb, 200);
	err = uosd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	const char buf[] = "The rain in spain";
	info("osd write");
	varlen_cdb_init(cdb);
	osd_write(cdb, 0, 27, 10, 5);
	uosd_cdb_write(fd, cdb, 200, buf, 10);
	err = uosd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	char bufout[] = "xxxxxxxxxxxxxxxxx";
	info("osd read");
	varlen_cdb_init(cdb);
	osd_read(cdb, 0, 27, 10, 5);
	uosd_cdb_read(fd, cdb, 200, bufout, 10);
	err = uosd_wait_response(fd, &key);
	info("response key %lx error %d, bufout %s", key, err, bufout);

	uosd_close(fd);
	return 0;
}

