/*
 * Test the use of the existing SG_IO interface to transport OSD commands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include "util/util.h"
#include "kernel_interface.h"
#include "user_interface_sgio.h"
#include "diskinfo.h"
#include "cdb_manip.h"

static const uint64_t PID = 0x10000LLU;
static const uint64_t OID = 0x10000LLU;
static const uint64_t CID = 0x10000LLU;
static const uint32_t LIST_ID = 0x10009LLU;
static const uint64_t PAGE = 0; 
static const uint16_t NUM_USER_OBJ = 1;
static const uint64_t OFFSET = 0;
static const int FLUSH_SCOPE = 2; /* Flush everything */
static const int OBJ_CAPACITY = 1<<30; /* 1 GB */
static const char WRITEDATA[] = "Write some data";
static const char WRITEDATA2[] = "Test #2";
static const char WRITEDATA3[] = "write data 3";

static int osd_command_get_attributes(struct osd_command *command,
                                      uint64_t pid, uint64_t oid)
{
	memset(command, 0, sizeof(*command));
	set_cdb_osd_get_attributes(command->cdb, pid, oid);
	command->cdb_len = OSD_CDB_SIZE;
	command->sense_len = OSD_MAX_SENSE;
	return 0;
}

static int bidi_test(int fd, uint64_t pid, uint64_t oid)
{
	int ret;
	struct osd_command command;
	uint64_t logical_length;
	struct attribute_id id = {
	    .page = 0x1,
	    .number = 0x82,  /* logical length (not used capacity) */
	    .len = sizeof(uint64_t),
	};
	uint8_t *p;

	osd_info(__func__);
	ret = osd_command_get_attributes(&command, pid, oid);
	if (ret) {
		osd_error_xerrno(ret, "%s: get_attributes failed", __func__);
		return 1;
	}
	ret = osd_command_attr_build(&command, &id, 1);
	if (ret) {
		osd_error_xerrno(ret, "%s: attr_build failed", __func__);
		return 1;
	}
	memset(command.indata, 0xaa, command.inlen_alloc);
	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit failed", __func__);
		return 1;
	}
	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

	/* verify retrieved list */
	osd_hexdump(command.indata, command.inlen_alloc);
	p = osd_command_attr_resolve(&command, &id, 1, 0);
	if (p == NULL) {
		printf("\n");
		return 1;
	}

	logical_length = ntohll(p);
	printf("%s: logical length 0x%016llx\n\n", __func__,
	       llu(logical_length));
	free(command.indata);
	free((void *)(uintptr_t) command.outdata);
	return 0;
}

static void iovec_write_test(int fd, uint64_t pid, uint64_t oid)
{
	struct osd_command command;
	const char buf1[] = "If iovec_write_test works,";
	const char buf2[] = " you will see this sentence.";
	char bufout[200];
	struct bsg_iovec vec[2];
	size_t tot_len;
	int ret;

	osd_info(__func__);
	vec[0].iov_base = (uint64_t)(uintptr_t) buf1;
	vec[0].iov_len = sizeof(buf1)-1;
	vec[1].iov_base = (uint64_t)(uintptr_t) buf2;
	vec[1].iov_len = sizeof(buf2);
	tot_len = sizeof(buf1)-1 + sizeof(buf2);
	memset(&command, 0, sizeof(command));
	set_cdb_osd_write(command.cdb, pid, oid, tot_len, 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.outlen = tot_len;
	command.outdata = vec;
	command.iov_outlen = 2;

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		return;
	}
	printf("%s: seemed to work\n", __func__);

	/* read it back, non-iov */
	memset(&command, 0, sizeof(command));
	memset(bufout, 0, sizeof(bufout));
	set_cdb_osd_read(command.cdb, pid, oid, sizeof(bufout), 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.inlen_alloc = sizeof(bufout);
	command.indata = bufout;

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret)
		osd_error("%s: submit_and_wait failed", __func__);
	printf("%s: read some bytes (%lu): %s\n\n", __func__,
	       command.inlen, bufout);
}

static void iovec_read_test(int fd, uint64_t pid, uint64_t oid)
{
	struct osd_command command;
	const char bufout[] = "A big line of data for iovec_read_test to get.";
	char buf1[21];
	char buf2[100];
	struct bsg_iovec vec[2];
	size_t tot_len;
	int ret;

	/* write it, non-iov */
	osd_info(__func__);
	memset(&command, 0, sizeof(command));
	set_cdb_osd_write(command.cdb, pid, oid, sizeof(bufout), 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.outlen = sizeof(bufout);
	command.outdata = bufout;
	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		return;
	}

	memset(buf1, 0, sizeof(buf1));
	memset(buf2, 0, sizeof(buf2));
	vec[0].iov_base = (uint64_t)(uintptr_t) buf1;
	vec[0].iov_len = sizeof(buf1)-1;
	vec[1].iov_base = (uint64_t)(uintptr_t) buf2;
	vec[1].iov_len = sizeof(buf2);
	tot_len = sizeof(buf1)-1 + sizeof(buf2);
	memset(&command, 0, sizeof(command));
	set_cdb_osd_read(command.cdb, pid, oid, tot_len, 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.inlen_alloc = tot_len;
	command.indata = vec;
	command.iov_inlen = 2;

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		return;
	}

	buf1[sizeof(buf1)-1] = '\0';  /* terminate partial string */
	printf("%s: read some bytes (%lu): %s + %s\n\n", __func__,
	       command.inlen, buf1, buf2);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;

	osd_set_progname(argc, argv); 

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		osd_error("%s: get drive error", __func__);
		return 1;
	}
	if (num_drives == 0) {
		osd_error("%s: no drives", __func__);
		return 1;
	}
	
	for (i=0; i<num_drives; i++) {

		printf("%s: drive %s name %s\n", progname, drives[i].chardev,
		       drives[i].targetname);
		fd = open(drives[i].chardev, O_RDWR);
		if (fd < 0) {
			osd_error_errno("%s: open %s", __func__, drives[i].chardev);
			return 1;
		}

		inquiry_sgio(fd);

		/*
		 * Clean the slate and make one partition.  Nothing works
		 * without a partition to put things into.
		 */
		format_osd_sgio(fd, OBJ_CAPACITY); 
		create_partition_sgio(fd, PID);


#if 1           /* These are all supposed to fail, for various reasons. */
		write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
		flush_osd_sgio(fd, FLUSH_SCOPE);
#endif

		
#if 1		/* Basic read / write seems to work */
		create_osd_sgio(fd, PID, OID, NUM_USER_OBJ+2);
		remove_osd_sgio(fd, PID, OID+1);

		write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
		read_osd_sgio(fd, PID, OID, OFFSET);
		write_osd_sgio(fd, PID, OID+2, WRITEDATA2, OFFSET);
		read_osd_sgio(fd, PID, OID+2, OFFSET);
	
		read_osd_sgio(fd, PID, OID, OFFSET);

#endif

#if 1           /* Testing iovec list. */
		create_osd_sgio(fd, PID, OID+3, 1);
		create_osd_sgio(fd, PID, OID+4, 1);

		iovec_write_test(fd, PID, OID+3);
		iovec_read_test(fd, PID, OID+4);

		remove_osd_sgio(fd, PID, OID+3);
		remove_osd_sgio(fd, PID, OID+4);
#endif

#if 1           /* Testing bidirectional operations. */
		create_osd_sgio(fd, PID, OID+5, 1);
		bidi_test(fd, PID, OID+5);
		remove_osd_sgio(fd, PID, OID+5);
#endif


#if 0		/* Testing stuff */
		create_osd_sgio(fd, PID, OID, NUM_USER_OBJ+3);
		write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
		write_osd_sgio(fd, PID, OID+1, WRITEDATA2, OFFSET);
		write_osd_sgio(fd, PID, OID+2, WRITEDATA3, OFFSET);
		get_attributes_sgio(fd, PID, OID);
		object_list_sgio(fd, PID, LIST_ID, OID);
		read_osd_sgio(fd, PID, OID+1, OFFSET);
		get_attribute_page_sgio(fd, PAGE, OFFSET);
#endif
		close(fd);
	}

	osd_free_drive_list(drives, num_drives);

	return 0;
}
