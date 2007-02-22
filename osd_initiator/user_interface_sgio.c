/*
 * Test the use of the existing SG_IO interface to transport OSD commands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "util/util.h"
#include "kernel_interface.h"
#include "user_interface_sgio.h"
#include "diskinfo.h"
#include "cdb_manip.h"

static const int INQUIRY_CDB_LEN = 6;
static const int INQUIRY_RSP_LEN = 80;

int inquiry_sgio(int fd)
{
	int ret;
	uint8_t inquiry_rsp[INQUIRY_RSP_LEN];
	struct osd_command command;

	osd_info("****** INQUIRY ******");
	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));

	memset(&command, 0, sizeof(command));
	cdb_build_inquiry(command.cdb, sizeof(inquiry_rsp));
	command.cdb_len = INQUIRY_CDB_LEN;
	command.indata = inquiry_rsp;
	command.inlen_alloc = sizeof(inquiry_rsp);

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	if (command.status !=0) 
		osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
		       command.status, command.sense_len, command.inlen);
	else
		osd_info("Inquiry successful");
	osd_hexdump(inquiry_rsp, command.inlen);

	printf("\n");
	return 0;
}

int create_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects)
{
	int i;
	int ret;
	struct osd_command command;

	osd_info("****** CREATE OBJECT ******");
	osd_info("Creating %u objects", num_user_objects);

	for (i=0; i < num_user_objects; i++) {
		osd_info("PID: %u OID: %u OBJ: %u/%u", 
			(uint)pid, (uint)requested_oid + i, i+1, num_user_objects);

		memset(&command, 0, sizeof(command));

		/* Create user objects one at a time */ 
		set_cdb_osd_create(command.cdb, pid, requested_oid + i, 1);
		command.cdb_len = OSD_CDB_SIZE;

		ret = osd_sgio_submit_and_wait(fd, &command);
		if (ret) {
			osd_error("%s: submit failed", __func__);
			return ret;
		}

		if (command.status != 0)
			osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
	       			command.status, command.sense_len, command.inlen);
		else
			osd_info("Object successfully created");
	
	}
	printf("\n");
	return 0;
}



int remove_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE OBJECT ******");
	osd_info("PID: %u OID: %u", (uint)pid, (uint)requested_oid);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_remove(command.cdb, pid, requested_oid);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);

	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	if (command.status != 0)
		osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
	      		 command.status, command.sense_len, command.inlen);
	else
		osd_info("Object successfully removed");

	printf("\n");
        return 0;
}


int create_osd_and_write_sgio(int fd, uint64_t pid, uint64_t requested_oid, const char *buf, uint64_t offset)
{
	create_osd_sgio(fd, pid, requested_oid, 1);
	write_osd_sgio(fd, pid, requested_oid, buf, offset);	
	return 0;
}

int write_osd_sgio(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** WRITE ******");
	osd_info("PID: %u OID: %u BUF: %s", (uint)pid, (uint)oid, buf);

	if (buf) {
 		memset(&command, 0, sizeof(command));
		set_cdb_osd_write(command.cdb, pid, oid, strlen(buf) + 1, offset);
		command.cdb_len = OSD_CDB_SIZE;
		command.outdata = buf;
		command.outlen = strlen(buf) + 1;

		ret = osd_sgio_submit_and_wait(fd, &command);
		if (ret) {
			osd_error("%s: submit failed", __func__);
			return ret;
		}
	
		if (command.status != 0)
			osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
		      		 command.status, command.sense_len, command.inlen);
		else
			osd_info("BUF successfully written");
	}
	else 
		osd_error("%s: no data sent", __func__); 

	printf("\n");
	return 0;
}

int read_osd_sgio(int fd, uint64_t pid, uint64_t oid, uint64_t offset)
{
	int ret;
	uint8_t buf[100];
	struct osd_command command;

	osd_info("****** READ ******");
	osd_info("PID: %u OID: %u EMPTY BUF: %s", (uint)pid, (uint)oid, buf);

	memset(&command, 0, sizeof(command));
	set_cdb_osd_read(command.cdb, pid, oid, sizeof(buf), offset);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);

	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	if (command.status != 0)
		osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
	       		command.status, command.sense_len, command.inlen);
 	else if (command.inlen > 0) 
		osd_info("BUF successfully read. BUF: %s", buf);
	else
		osd_info("Nothing read. Attempt successful");

	printf("\n");
	return 0;
}

int format_osd_sgio(int fd, int capacity)
{
        int ret;
        struct osd_command command;

        osd_info("****** FORMAT ******"); 
	osd_info("Capacity: %i", capacity);

	memset(&command, 0, sizeof(command));
        set_cdb_osd_format_osd(command.cdb, capacity);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);	
	
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	if (command.status != 0) 
		osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
		       command.status, command.sense_len, command.inlen);
	else 
		osd_info("Format successful");

	printf("\n");
        return 0;
}

int flush_osd_sgio(int fd, int flush_scope)
{
        int ret;
        struct osd_command command;

        osd_info("****** FLUSH ******"); 

	memset(&command, 0, sizeof(command));
        set_cdb_osd_flush_osd(command.cdb, flush_scope);   
        command.cdb_len = OSD_CDB_SIZE;
	
	ret = osd_sgio_submit_and_wait(fd, &command);

	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	if (command.status != 0) 
		osd_error("%s: status: %u sense len: %u inlen: %zu", __func__,
		       command.status, command.sense_len, command.inlen);
	else
		osd_info("Flush successful");

	printf("\n");
        return 0; 
}

