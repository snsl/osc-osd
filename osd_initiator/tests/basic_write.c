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

int format_osd(int fd, int cdb_len, int capacity);
int create_osd(int fd, int cdb_len, uint64_t pid, uint64_t requested_oid,
		uint16_t num_user_objects);
int write_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
		uint64_t buf_len, uint64_t offset, const char * buf[]);
int read_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
		uint64_t buf_len, uint64_t offset, char bufout[]);
int inquiry(int fd);
int flush_osd(int fd, int cdb_len);


int main(int argc, char *argv[])
{
	int cdb_len = OSD_CDB_SIZE;
	int fd, i;  
	const char * buf[10];
	char outbuf[10] = "xxxxxxx";
	
	set_progname(argc, argv); 
	fd = dev_osd_open("/dev/sua");
	if (fd < 0) {
		error_errno("%s: open /dev/sua", __func__);
		return 1;
	}

	inquiry(fd);

#if 1
	info("sleeping 2 before flush");
	sleep(2);
	flush_osd(fd, cdb_len);  /*this is a no op no need to flush*/
	info("sleeping 2 before format");
	sleep(2);
#endif

#if 0
	format_osd(fd, cdb_len, 1<<30);  /*1GB*/
	create_osd(fd, cdb_len, FIRST_USER_PARTITION, FIRST_USER_OBJECT, 1); 
	printf("Format and create work, need to fix up buf for write to work\n");
	return 0;
	*buf = "The Rain in Spain"; /*buf only has 10 bytes allocated though?*/
	write_osd(fd, cdb_len, 0, FIRST_USER_PARTITION, FIRST_USER_OBJECT, 0, buf); 
	read_osd(fd, cdb_len, 0, 27, 20, 5, outbuf);


	// read_osd(fd, cdb_len, 16, 27, 20, 5, "xxxxxxxxxxxxxxxx");
#endif

#if 0
	char bad_bufout[] = "xxxxxxxxxxxxxxxxx";
	info("osd read bad");
	varlen_cdb_init(cdb);
	set_cdb_osd_read(cdb, 16, 27, 10, 5);  // magic bad pid causes error 
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
#endif

	dev_osd_close(fd);
	return 0;
}

// fd, cdb_len, capacity 
int format_osd(int fd, int cdb_len, int capacity)
{
	int err;
        uint64_t key;
	uint8_t cdb[cdb_len];
	struct osd_command command;
	enum data_direction dir = DMA_NONE;	
	info("********** OSD FORMAT **********");
	varlen_cdb_init(cdb);
	set_cdb_osd_format_osd(cdb, capacity);

	command.cdb = cdb;
	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command, dir); 

	err = dev_osd_wait_response(fd, &key);	
	info("response key %lx error %d", key, err);
	return err;
}

// fd, cdb_len, partition ID, requested user object ID, number of user objects 
int create_osd(int fd, int cdb_len, uint64_t pid, uint64_t requested_oid,
		uint16_t num_user_objects)
{
	int err;
	uint64_t key;
	uint8_t cdb[cdb_len];
	struct osd_command command;
	enum data_direction dir = DMA_NONE;
	info("********** OSD CREATE **********");
	varlen_cdb_init(cdb);
	set_cdb_osd_create(cdb, pid, requested_oid, num_user_objects);

	command.cdb = cdb;
	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command, dir); 

	err = dev_osd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);
	return err;
}

// fd, cdb_len, partition ID, user object ID, length of argument, starting byte address, argument
int write_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
		uint64_t buf_len, uint64_t offset, const char * buf[])
{
	int err;
	uint64_t key;
	uint8_t cdb[cdb_len];
	struct osd_command command;
	enum data_direction dir = DMA_TO_DEVICE;

	info("********** OSD WRITE **********");
	varlen_cdb_init(cdb);
	set_cdb_osd_write(cdb, pid, oid, buf_len, offset);

	command.cdb = cdb;
	command.cdb_len = cdb_len;	
	command.outdata = buf;
	command.outlen = buf_len;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command, dir);

	err = dev_osd_wait_response(fd, &key);
	info("argument: '%s'", *buf);
	info("response key %lx error %d", key, err);
	return err;
}

// fd, cdb_len, partition ID, user object ID, length of argument, starting byte address, argument
int read_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
		uint64_t buf_len, uint64_t offset, char bufout[])
{
	int err;
	struct dev_response resp;
	uint8_t cdb[cdb_len];
	struct osd_command command;
	enum data_direction dir = DMA_FROM_DEVICE;

	info("********** OSD READ **********");
	varlen_cdb_init(cdb);
	set_cdb_osd_read(cdb, pid, oid, buf_len, offset);

	command.cdb = cdb;
	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = bufout;
	command.inlen = buf_len;
	osd_submit_command(fd, &command, dir);

	//err = dev_osd_wait_response2(fd, &resp);
	info("argument: '%s'", bufout);
	info("response key %lx error %d, bufout %s, sense len %d", resp.key,
	     resp.error, bufout, resp.sense_buffer_len);
	return err;
}

int inquiry(int fd)
{
	int err;
	uint64_t key;
	uint8_t inquiry_rsp[80], cdb[200];
	struct osd_command command;
	enum data_direction dir = DMA_FROM_DEVICE;

	info("********** INQUIRY **********");
	info("inquiry");
	cdb_build_inquiry(cdb);
	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));

	command.cdb = cdb;
	command.cdb_len = 6;	
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = inquiry_rsp;
	command.inlen = sizeof(inquiry_rsp);
	osd_submit_command(fd, &command, dir); 

	info("waiting for response");
	err = dev_osd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);
	hexdump(inquiry_rsp, sizeof(inquiry_rsp));
	fflush(0);

	return err;
}

int flush_osd(int fd, int cdb_len)
{
	int err;
	uint8_t cdb[cdb_len];
	uint64_t key;
	struct osd_command command;
	enum data_direction dir = DMA_NONE;

	info("********** OSD FLUSH OSD **********");
	varlen_cdb_init(cdb);
	set_cdb_osd_flush_osd(cdb, 2);   /* flush everything: cdb, flush_scope */ 

	command.cdb = cdb;
	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command, dir); 

	info("waiting for response");
	err = dev_osd_wait_response(fd, &key);
	info("response key %lx error %d", key, err);

	return err;
}
