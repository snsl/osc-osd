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
#include "interface.h"
#include "diskinfo.h"
#include "cdb_manip.h"

static const uint64_t pid = 0x10000LLU;
static const uint64_t oid = 0x10003LLU;

static void cdb_build_inquiry(uint8_t *cdb, uint8_t outlen)
{
	cdb[0] = INQUIRY;
	cdb[4] = outlen;
}

static int inquiry(int fd)
{
	int ret;
	uint8_t inquiry_rsp[80];
	struct osd_command command, *cmp;

	info("inquiry");
	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));

	memset(&command, 0, sizeof(command));
	cdb_build_inquiry(command.cdb, sizeof(inquiry_rsp));
	command.cdb_len = 6;
	command.indata = inquiry_rsp;
	command.inlen_alloc = sizeof(inquiry_rsp);

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		error("%s: submit failed", __func__);
		return ret;
	}

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);
	hexdump(inquiry_rsp, command.inlen);

	return 0;
}

static int create_osd(int fd)
{
	int ret;
	struct osd_command command, *cmp;

	info("create");

	memset(&command, 0, sizeof(command));
	set_cdb_osd_create(command.cdb, pid, oid, 1);
	command.cdb_len = OSD_CDB_SIZE;

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		error("%s: submit failed", __func__);
		return ret;
	}

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

	return 0;
}

static int write_osd(int fd)
{
	int ret;
	const char buf[] = "Some write data.\n";
	struct osd_command command, *cmp;

	info("write");

	memset(&command, 0, sizeof(command));
	set_cdb_osd_write(command.cdb, pid, oid, strlen(buf)+1, 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.outdata = buf;
	command.outlen = strlen(buf) + 1;

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		error("%s: submit failed", __func__);
		return ret;
	}

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);
	printf("%s: wrote: %s", __func__, buf);

	return 0;
}

static int read_osd(int fd)
{
	int ret;
	uint8_t buf[100];
	struct osd_command command;

	info("read");

	memset(&command, 0, sizeof(command));
	set_cdb_osd_read(command.cdb, pid, oid, sizeof(buf), 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	buf[0] = '\0';

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		error("%s: submit failed", __func__);
		return ret;
	}

	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);
	printf("%s: hexdump of read data\n", __func__);
	hexdump(buf, command.inlen);
	if (command.inlen > 0)
	    printf("%s: read back: %s", __func__, buf);

	return 0;
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;

	set_progname(argc, argv); 

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		error("%s: get drive error", __func__);
		return 1;
	}
	if (num_drives == 0) {
		error("%s: no drives", __func__);
		return 1;
	}
	
	for (i=0; i<num_drives; i++) {

		printf("%s: drive %s name %s\n", progname, drives[i].chardev,
		       drives[i].targetname);
		fd = open(drives[i].chardev, O_RDWR);
		if (fd < 0) {
			error_errno("%s: open %s", __func__, drives[i].chardev);
			return 1;
		}

		inquiry(fd);
		create_osd(fd);
		write_osd(fd);
		read_osd(fd);

		close(fd);
	}

	osd_free_drive_list(drives, num_drives);

	return 0;
}

