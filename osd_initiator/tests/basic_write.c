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
#include "interface.h"
#include "osd_hdr.h"

#define OSD_CDB_SIZE 200
#define VARLEN_CDB 0x7f
#define TIMESTAMP_ON 0x0
#define TIMESTAMP_OFF 0x7f

#define BUFSIZE 1024

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
		uint64_t buf_len, uint64_t offset, const char *buf);
int read_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
		uint64_t buf_len, uint64_t offset, char bufout[]);
int inquiry(int fd);
int flush_osd(int fd, int cdb_len);


int main(int argc, char *argv[])
{
	int cdb_len = OSD_CDB_SIZE;
	int fd, i;  
	char *buf;
	int ret;
	int len;
		
	set_progname(argc, argv); 
	fd = dev_osd_open("/dev/sua");
	if (fd < 0) {
		error_errno("%s: open /dev/sua", __func__);
		return 1;
	}

	inquiry(fd);
	flush_osd(fd, cdb_len);  /*this is a no op no need to flush, on target files are
						opened read/written and closed each time so 
						objects are in a sense flushed every time*/

	ret = format_osd(fd, cdb_len, 1<<30);  /*1GB*/
	if(ret != 0)
		return ret;
	ret = create_osd(fd, cdb_len, FIRST_USER_PARTITION, FIRST_USER_OBJECT, 1); 
	if(ret != 0)
		return ret;
	
	buf = malloc(BUFSIZE);
	memset(buf, '\0', BUFSIZE);
	strcpy(buf, "The Rain in Spain");
	
	printf("Going to write: %s\n", buf);
	len = strlen(buf)+1;
	ret =write_osd(fd, cdb_len, FIRST_USER_PARTITION, FIRST_USER_OBJECT, len, 0, buf); 	
	if(ret != 0)
		return ret;
	
	memset(buf, '\0', BUFSIZE);
	printf("Buf should be cleared: %s\n", buf);
	ret = read_osd(fd, cdb_len, FIRST_USER_PARTITION, FIRST_USER_OBJECT, len, 0, buf);
	if(ret != 0)
		return ret;
	printf("Read back: %s\n", buf);
	
	dev_osd_close(fd);
	return 0;
}

/* fd, cdb_len, capacity  */
int format_osd(int fd, int cdb_len, int capacity)
{
	int err;
        uint64_t key;
	struct osd_command command;
	struct suo_response resp;	
	
	info("********** OSD FORMAT **********");

	varlen_cdb_init(command.cdb);
	set_cdb_osd_format_osd(command.cdb, capacity);

	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command); 

	err = dev_osd_wait_response(fd, &resp);
	info("response key %lx error %d,sense len %d", resp.key, resp.error, resp.sense_buffer_len); 
	
	if(err != 0)
		return err;
	
	if(resp.sense_buffer_len){
		dev_show_sense(resp.sense_buffer, resp.sense_buffer_len);
		err = -1;
	}
	return err;
}

/* fd, cdb_len, partition ID, requested user object ID, number of user objects  */
int create_osd(int fd, int cdb_len, uint64_t pid, uint64_t requested_oid,
		uint16_t num_user_objects)
{
	int err;
	struct osd_command command; 
	struct suo_response resp;

	info("********** OSD CREATE **********");
	varlen_cdb_init(command.cdb);
	set_cdb_osd_create(command.cdb, pid, requested_oid, num_user_objects);

	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command); 
	
	err = dev_osd_wait_response(fd, &resp);
	info("response key %lx error %d,sense len %d", resp.key, resp.error, resp.sense_buffer_len); 
	
	if(err != 0)
		return err; 
	
	if(resp.sense_buffer_len){
		dev_show_sense(resp.sense_buffer, resp.sense_buffer_len);
		err = -1;
	}
	return err;
}

/* fd, cdb_len, partition ID, user object ID, length of argument, starting byte address, argument */
int write_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
		uint64_t buf_len, uint64_t offset, const char *buf)
{
	int err;
	struct suo_response resp;
	struct osd_command command;

	info("********** OSD WRITE **********");
	varlen_cdb_init(command.cdb);
	set_cdb_osd_write(command.cdb, pid, oid, buf_len, offset);

	command.cdb_len = cdb_len;	 
	command.outdata = buf;
	command.outlen = buf_len;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command);

	err = dev_osd_wait_response(fd, &resp);
	info("argument: '%s'", buf);
	info("response key %lx error %d,sense len %d", resp.key,
	     resp.error, resp.sense_buffer_len); 
	
	if(err != 0)
		return err;
	
	if(resp.sense_buffer_len){
		dev_show_sense(resp.sense_buffer, resp.sense_buffer_len);
		err = -1;
	}
	return err;
}

/* fd, cdb_len, partition ID, user object ID, length of argument, starting byte address, argument */
int read_osd(int fd, int cdb_len, uint64_t pid, uint64_t oid,
		uint64_t buf_len, uint64_t offset, char bufout[])
{
	int err;
	struct suo_response resp;
	struct osd_command command;

	info("********** OSD READ **********");
	varlen_cdb_init(command.cdb);
	set_cdb_osd_read(command.cdb, pid, oid, buf_len, offset);

	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = bufout;
	command.inlen = buf_len;
	osd_submit_command(fd, &command);

	err = dev_osd_wait_response(fd, &resp);
	info("argument: '%s'", bufout);
	info("response key %lx error %d, bufout %s, sense len %d", resp.key,
	     resp.error, bufout, resp.sense_buffer_len);
	
	if(err != 0)
		return err;
	
	if(resp.sense_buffer_len){
		dev_show_sense(resp.sense_buffer, resp.sense_buffer_len);
		err = -1; 
	}
	return err;
}

int inquiry(int fd)
{
	int err;
	uint64_t key;
	uint8_t inquiry_rsp[80];
	struct osd_command command;
	struct suo_response resp;

	info("********** INQUIRY **********");
	info("inquiry");
	cdb_build_inquiry(command.cdb);
	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));

	command.cdb_len = 6;	
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = inquiry_rsp;
	command.inlen = sizeof(inquiry_rsp);
	osd_submit_command(fd, &command); 

	info("waiting for response");
	err = dev_osd_wait_response(fd, &resp);
	info("response key %lx error %d, sense len %d", resp.key, resp.error, resp.sense_buffer_len);
	if(err != 0)
		return err;
	if(resp.sense_buffer_len){
		dev_show_sense(resp.sense_buffer, resp.sense_buffer_len);
		err = -1; 
	}
	hexdump(inquiry_rsp, sizeof(inquiry_rsp));
	fflush(0);

	return err;
}

int flush_osd(int fd, int cdb_len)
{
	int err;
	uint64_t key;
	struct osd_command command;
	struct suo_response resp;

	info("********** OSD FLUSH OSD **********");
	varlen_cdb_init(command.cdb);
	set_cdb_osd_flush_osd(command.cdb, 2);   /* flush everything: cdb, flush_scope */ 

	command.cdb_len = cdb_len;
	command.outdata = NULL;
	command.outlen = 0;
	command.indata = NULL;
	command.inlen = 0;
	osd_submit_command(fd, &command); 

	info("waiting for response");
	err = dev_osd_wait_response(fd, &resp);
	info("response key %lx error %d, sense len %d", resp.key,  resp.error, resp.sense_buffer_len);
 	
	if(err != 0)
		return err;
	
	if(resp.sense_buffer_len){
		dev_show_sense(resp.sense_buffer, resp.sense_buffer_len);
		err = -1; 
	}
	return err;
}
