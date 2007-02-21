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

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);
	osd_hexdump(inquiry_rsp, command.inlen);

	return 0;
}

int create_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects)
{
	int ret;
	struct osd_command command;

	osd_info("****** CREATE OBJECT ******");

	memset(&command, 0, sizeof(command));
	set_cdb_osd_create(command.cdb, pid, requested_oid, num_user_objects);
	command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

	return 0;
}

int remove_osd_sgio(int fd, uint64_t pid, uint64_t requested_oid)
{
        int ret;
        struct osd_command command;

	osd_info("****** REMOVE OBJECT ******");

	memset(&command, 0, sizeof(command));
        set_cdb_osd_remove(command.cdb, pid, requested_oid);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);

	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

        return 0;
}


int write_osd_sgio(int fd, uint64_t pid, uint64_t oid, const char *buf, uint64_t offset)
{
	int ret;
	struct osd_command command;
	
	osd_info("****** WRITE ******");

	if (buf)
	{
		/* printf("%s: PID: %u OID: %u Data: %s ", __func__, pid, oid, buf); */

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
	
		printf("%s: status %u sense len %u inlen %zu\n", __func__,
		       command.status, command.sense_len, command.inlen);
	}
	else
	{
		osd_error("%s: no data sent", __func__); 
	}
	return 0;
}

int read_osd_sgio(int fd, uint64_t pid, uint64_t oid, uint64_t offset)
{
	int ret;
	uint8_t buf[100];
	struct osd_command command;

	osd_info("****** READ ******");
	/* printf("%s: PID: %u OID: %u \n", __func__, pid, oid); */

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

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);
	/*
	printf("%s: hexdump of read data\n", __func__);
	osd_hexdump(buf, command.inlen); 
	*/
	(command.inlen && command.indata) > 0 ? printf("%s: read back: %s", __func__, buf) : printf("%s: nothing read back", __func__);

	return 0;
}

int format_osd_sgio(int fd, int capacity)
{
        int ret;
        struct osd_command command;

        osd_info("****** FORMAT ******"); 

	memset(&command, 0, sizeof(command));
        set_cdb_osd_format_osd(command.cdb, capacity);
        command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);	
	
	if (ret) {
		osd_error("%s: submit failed", __func__);
		return ret;
	}

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

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

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

        return 0; 
}

