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
#include "kernel_interface.h"
#include "user_interface.h"
#include "cdb_manip.h"

/* fd, cdb_len, capacity  */
int format_osd(int fd, int cdb_len, int capacity)
{
	int err;
        uint64_t key;
	struct osd_command command;
	struct suo_response resp;	
	
	info("********** OSD FORMAT **********");

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
int create_object(int fd, int cdb_len, uint64_t pid, uint64_t requested_oid,
		uint16_t num_user_objects)
{
	int err;
	struct osd_command command; 
	struct suo_response resp;

	info("********** CREATE OBJECT **********");
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


int remove_object(int fd, int cdb_len, uint64_t pid, uint64_t requested_oid)
{
	int err;
	struct osd_command command; 
	struct suo_response resp;

	info("********** REMOVE OBJECT **********");
	set_cdb_osd_remove(command.cdb, pid, requested_oid);

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
		uint64_t buf_len, uint64_t offset, const char* bufout)
{
	int err;
	struct suo_response resp;
	struct osd_command command;

	info("********** OSD READ **********");
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

int inquiry_osd(int fd)
{
	int err;
	uint64_t key;
	uint8_t inquiry_rsp[80];
	struct osd_command command;
	struct suo_response resp;

	info("********** INQUIRY **********");
	info("inquiry");
	cdb_build_inquiry(command.cdb, (uint8_t)80);
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
