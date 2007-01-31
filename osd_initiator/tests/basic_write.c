/*
 * Test the chardev transport to the kernel.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <scsi/scsi.h>

#include <stdint.h>
#include <sys/types.h>

#include "util/util.h"
#include "libosd/interface.h"
#include "libosd/osd_hdr.h"

#define OSD_CDB_SIZE 200
#define VARLEN_CDB 0x7f
#define TIMESTAMP_ON 0x0
#define TIMESTAMP_OFF 0x7f

/*
 * Initializes a new varlen cdb.
 */
static void varlen_cdb_init(uint8_t *cdb)
{
	memset(cdb, 0, OSD_CDB_SIZE);
	cdb[0] = VARLEN_CDB;
	/* we do not support ACA or LINK in control byte cdb[1], leave as 0 */
	cdb[7] = OSD_CDB_SIZE - 8;
	cdb[11] = 2 << 4;  /* get attr page and set value see spec 5.2.2.2 */
	cdb[12] = TIMESTAMP_OFF; /* Update timestamps based on action 5.2.8 */
}

static void cdb_build_inquiry(uint8_t *cdb)
{
	memset(cdb, 0, 6);
	cdb[0] = INQUIRY;
	cdb[4] = 80;
}

int main(int argc, char *argv[])
{
	uint8_t cdb[200];
	uint8_t inquiry_rsp[80];
	uint64_t key; 
	int fd, err, i;  
	struct dev_response resp;

	set_progname(argc, argv); 

	fd = dev_osd_open("/dev/sua");

#if 1
	for (i=0; i<2; i++)
	{
		info("inquiry");
		memset(inquiry_rsp, 0xaa, 80);
		cdb_build_inquiry(cdb);
		memset(inquiry_rsp, 0, sizeof(inquiry_rsp));
		dev_osd_read(fd, cdb, 6, inquiry_rsp, sizeof(inquiry_rsp));

		/* now wait for the result */
		info("waiting for response");
		err = dev_osd_wait_response(fd, &key);
		info("response key %lx error %d", key, err);
		hexdump(inquiry_rsp, sizeof(inquiry_rsp));
		fflush(0);	
	}

	info("sleeping 2 before flush");
	sleep(2);

	info("osd flush osd");
	varlen_cdb_init(cdb);
	set_cdb_osd_flush_osd(cdb, 2);   /* flush everything */
	dev_osd_write_nodata(fd, cdb, 200);
	info("waiting for response");
	err = dev_osd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	info("sleeping 2 before format");
	sleep(2);
#endif

#if 1
	info("osd format osd");
	varlen_cdb_init(cdb);
	set_cdb_osd_format_osd(cdb, 1<<30);   /* capacity 1 GB */
	dev_osd_write_nodata(fd, cdb, 200);
	err = dev_osd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	info("osd create");
	varlen_cdb_init(cdb);
	set_cdb_osd_create(cdb, 0, 27, 1);
	dev_osd_write_nodata(fd, cdb, 200);
	err = dev_osd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	const char buf[] = "The rain in spain";
	info("osd write");
	varlen_cdb_init(cdb);
	set_cdb_osd_write(cdb, 0, 27, 10, 5);
	dev_osd_write(fd, cdb, 200, buf, 10);
	err = dev_osd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	char bufout[] = "xxxxxxxxxxxxxxxxx";
	info("osd read");
	varlen_cdb_init(cdb);
	set_cdb_osd_read(cdb, 0, 27, 10, 5);
	dev_osd_read(fd, cdb, 200, bufout, 10);
	err = dev_osd_wait_response2(fd, &resp);
	info("response key %lx error %d, bufout %s, sense len %d", resp.key,
	     resp.error, bufout, resp.sense_buffer_len);
#endif

	char bad_bufout[] = "xxxxxxxxxxxxxxxxx";
	info("osd read bad");
	varlen_cdb_init(cdb);
	set_cdb_osd_read(cdb, 16, 27, 10, 5);  /* magic bad pid causes error */
	/*
	 * Also need a way to get back how much data was written.  It could
	 * be a partial read, with good data, but sense saying we asked for
	 * more than was in the object.
	 */
	dev_osd_read(fd, cdb, 200, bad_bufout, 10);
	err = dev_osd_wait_response2(fd, &resp);
	info("response key %lx error %d, sense len %d", resp.key, resp.error,
	     resp.sense_buffer_len);
	if (resp.sense_buffer_len)
		dev_show_sense(resp.sense_buffer, resp.sense_buffer_len);

	dev_osd_close(fd);
	return 0;
}

