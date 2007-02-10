/*
 * Test the chardev transport to the kernel.
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
#include "osd_hdr.h"

#define OSD_CDB_SIZE 200
#define VARLEN_CDB 0x7f
#define TIMESTAMP_ON 0x0
#define TIMESTAMP_OFF 0x7f

static const uint64_t pid = 0x10000LLU;
static const uint64_t oid = 0x10003LLU;

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

static int inquiry(int fd)
{
	int ret;
	uint8_t inquiry_rsp[80], cdb[OSD_CDB_SIZE], sense[252];
	struct sg_io_hdr sg;

	info("inquiry");
	cdb_build_inquiry(cdb);
	memset(inquiry_rsp, 0xaa, sizeof(inquiry_rsp));

	memset(&sg, 0, sizeof(sg));
	sg.interface_id = 'S';
	sg.dxfer_direction = SG_DXFER_FROM_DEV;
	sg.cmd_len = 6;
	sg.mx_sb_len = sizeof(sense);
	sg.dxfer_len = sizeof(inquiry_rsp);
	sg.dxferp = inquiry_rsp;
	sg.cmdp = cdb;
	sg.sbp = sense;
	sg.timeout = 3000;
	ret = write(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: write", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short write, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	ret = read(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: read", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short read, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	printf("%s: status %u sense len %u resid %d\n", __func__, sg.status,
	       sg.sb_len_wr, sg.resid);
	hexdump(inquiry_rsp, sizeof(inquiry_rsp) - sg.resid);

	return 0;
}

static int create_osd(int fd)
{
	int ret;
	uint8_t cdb[OSD_CDB_SIZE], sense[252];
	struct sg_io_hdr sg;

	info("create");
	varlen_cdb_init(cdb);
	set_cdb_osd_create(cdb, pid, oid, 1);

	memset(&sg, 0, sizeof(sg));
	sg.interface_id = 'S';
	sg.dxfer_direction = SG_DXFER_FROM_DEV;
	sg.cmd_len = OSD_CDB_SIZE;
	sg.mx_sb_len = sizeof(sense);
	sg.cmdp = cdb;
	sg.sbp = sense;
	sg.timeout = 3000;
	ret = write(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: write", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short write, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	ret = read(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: read", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short read, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	printf("%s: status %u sense len %u resid %d\n", __func__, sg.status,
	       sg.sb_len_wr, sg.resid);

	return 0;
}

static int write_osd(int fd)
{
	int ret;
	uint8_t cdb[OSD_CDB_SIZE], sense[252];
	const char buf[] = "Some write data.\n";
	struct sg_io_hdr sg;

	info("write");
	varlen_cdb_init(cdb);
	set_cdb_osd_write(cdb, pid, oid, strlen(buf)+1, 0);

	memset(&sg, 0, sizeof(sg));
	sg.interface_id = 'S';
	sg.dxfer_direction = SG_DXFER_TO_DEV;
	sg.cmd_len = OSD_CDB_SIZE;
	sg.mx_sb_len = sizeof(sense);
	sg.dxfer_len = strlen(buf)+1;
	sg.dxferp = (void *)(uintptr_t) buf;  /* discard const */
	sg.cmdp = cdb;
	sg.sbp = sense;
	sg.timeout = 3000;
	ret = write(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: write", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short write, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	ret = read(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: read", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short read, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	printf("%s: status %u sense len %u resid %d\n", __func__, sg.status,
	       sg.sb_len_wr, sg.resid);
	printf("%s: wrote: %s", __func__, buf);

	return 0;
}

static int read_osd(int fd)
{
	int ret;
	uint8_t cdb[OSD_CDB_SIZE], sense[252];
	uint8_t buf[100];
	struct sg_io_hdr sg;

	info("read");
	varlen_cdb_init(cdb);
	set_cdb_osd_read(cdb, pid, oid, sizeof(buf), 0);

	buf[0] = '\0';
	memset(&sg, 0, sizeof(sg));
	sg.interface_id = 'S';
	sg.dxfer_direction = SG_DXFER_FROM_DEV;
	sg.cmd_len = OSD_CDB_SIZE;
	sg.mx_sb_len = sizeof(sense);
	sg.dxfer_len = sizeof(buf);
	sg.dxferp = buf;
	sg.cmdp = cdb;
	sg.sbp = sense;
	sg.timeout = 3000;
	ret = write(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: write", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short write, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	ret = read(fd, &sg, sizeof(sg));
	if (ret < 0) {
		error_errno("%s: read", __func__);
		return -errno;
	}
	if (ret != sizeof(sg)) {
		error("%s: short read, %d not %zu", __func__, ret, sizeof(sg));
		return 1;
	}

	printf("%s: status %u sense len %u resid %d\n", __func__, sg.status,
	       sg.sb_len_wr, sg.resid);
	hexdump(buf, sizeof(buf) - sg.resid);
	printf("%s: read back: %s", __func__, buf);

	return 0;
}


int main(int argc, char *argv[])
{
	int fd;  
	
	set_progname(argc, argv); 
	fd = open("/dev/sg1", O_RDWR);
	if (fd < 0) {
		error_errno("%s: open /dev/sg1", __func__);
		return 1;
	}

	inquiry(fd);
	create_osd(fd);
	write_osd(fd);
	read_osd(fd);
	close(fd);
	return 0;
}

